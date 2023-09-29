#include "UndoableModelActions.hpp"

#include <OpenSimCreator/Bindings/SimTKHelpers.hpp>
#include <OpenSimCreator/Graphics/OpenSimDecorationOptions.hpp>
#include <OpenSimCreator/Graphics/OpenSimDecorationGenerator.hpp>
#include <OpenSimCreator/Model/BasicModelStatePair.hpp>
#include <OpenSimCreator/Model/UndoableModelStatePair.hpp>
#include <OpenSimCreator/Registry/ComponentRegistry.hpp>
#include <OpenSimCreator/Registry/StaticComponentRegistries.hpp>
#include <OpenSimCreator/Simulation/ForwardDynamicSimulation.hpp>
#include <OpenSimCreator/Simulation/ForwardDynamicSimulatorParams.hpp>
#include <OpenSimCreator/Simulation/Simulation.hpp>
#include <OpenSimCreator/Simulation/StoFileSimulation.hpp>
#include <OpenSimCreator/UI/Middleware/MainUIStateAPI.hpp>
#include <OpenSimCreator/UI/Tabs/ModelEditorTab.hpp>
#include <OpenSimCreator/UI/Tabs/LoadingTab.hpp>
#include <OpenSimCreator/UI/Tabs/SimulatorTab.hpp>
#include <OpenSimCreator/UI/Tabs/PerformanceAnalyzerTab.hpp>
#include <OpenSimCreator/UI/Widgets/ObjectPropertiesEditor.hpp>
#include <OpenSimCreator/Utils/OpenSimHelpers.hpp>

#include <OpenSim/Common/Component.h>
#include <OpenSim/Common/ComponentList.h>
#include <OpenSim/Common/ComponentPath.h>
#include <OpenSim/Common/ComponentSocket.h>
#include <OpenSim/Common/Exception.h>
#include <OpenSim/Common/ModelDisplayHints.h>
#include <OpenSim/Common/Property.h>
#include <OpenSim/Common/PropertyObjArray.h>
#include <OpenSim/Common/Set.h>
#include <OpenSim/Simulation/Model/ContactGeometry.h>
#include <OpenSim/Simulation/Model/GeometryPath.h>
#include <OpenSim/Simulation/Model/ModelVisualPreferences.h>
#include <OpenSim/Simulation/Model/OffsetFrame.h>
#include <OpenSim/Simulation/Model/PathActuator.h>
#include <OpenSim/Simulation/Model/PathPointSet.h>
#include <OpenSim/Simulation/Model/JointSet.h>
#include <OpenSim/Simulation/Model/HuntCrossleyForce.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/Model/PhysicalFrame.h>
#include <OpenSim/Simulation/Model/PhysicalOffsetFrame.h>
#include <OpenSim/Simulation/SimbodyEngine/Body.h>
#include <OpenSim/Simulation/SimbodyEngine/Coordinate.h>
#include <OpenSim/Simulation/SimbodyEngine/Joint.h>
#include <OpenSim/Simulation/SimbodyEngine/WeldJoint.h>
#include <oscar/Graphics/MeshCache.hpp>
#include <oscar/Platform/App.hpp>
#include <oscar/Platform/Log.hpp>
#include <oscar/Platform/os.hpp>
#include <oscar/Utils/FilesystemHelpers.hpp>
#include <oscar/Utils/ParentPtr.hpp>
#include <oscar/Utils/UID.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
#include <typeinfo>
#include <utility>
#include <vector>

// helper functions
namespace
{
    void OpenOsimInLoadingTab(osc::ParentPtr<osc::MainUIStateAPI> const& api, std::filesystem::path p)
    {
        api->addAndSelectTab<osc::LoadingTab>(api, std::move(p));
    }

    void DoOpenFileViaDialog(osc::ParentPtr<osc::MainUIStateAPI> const& api)
    {
        std::optional<std::filesystem::path> const maybePath = osc::PromptUserForFile("osim");

        if (maybePath)
        {
            OpenOsimInLoadingTab(api, *maybePath);
        }
    }

    std::optional<std::filesystem::path> PromptSaveOneFile()
    {
        return osc::PromptUserForFileSaveLocationAndAddExtensionIfNecessary("osim");
    }

    bool IsAnExampleFile(std::filesystem::path const& path)
    {
        return osc::IsSubpath(osc::App::resource("models"), path);
    }

    std::optional<std::string> TryGetModelSaveLocation(OpenSim::Model const& m)
    {
        if (std::string const& backing_path = m.getInputFileName();
            !backing_path.empty() && backing_path != "Unassigned")
        {
            // the model has an associated file
            //
            // we can save over this document - *IF* it's not an example file
            if (IsAnExampleFile(backing_path))
            {
                std::optional<std::filesystem::path> const maybePath = PromptSaveOneFile();
                return maybePath ? std::optional<std::string>{maybePath->string()} : std::nullopt;
            }
            else
            {
                return backing_path;
            }
        }
        else
        {
            // the model has no associated file, so prompt the user for a save
            // location
            std::optional<std::filesystem::path> const maybePath = PromptSaveOneFile();
            return maybePath ? std::optional<std::string>{maybePath->string()} : std::nullopt;
        }
    }

    bool TrySaveModel(OpenSim::Model const& model, std::string const& save_loc)
    {
        try
        {
            model.print(save_loc);
            osc::log::info("saved model to %s", save_loc.c_str());
            return true;
        }
        catch (OpenSim::Exception const& ex)
        {
            osc::log::error("error saving model: %s", ex.what());
            return false;
        }
    }

    // create a "standard" OpenSim::Joint
    std::unique_ptr<OpenSim::Joint> MakeJoint(
        osc::BodyDetails const& details,
        OpenSim::Body const& b,
        OpenSim::Joint const& jointPrototype,
        OpenSim::PhysicalFrame const& selectedPf)
    {
        std::unique_ptr<OpenSim::Joint> copy = osc::Clone(jointPrototype);
        copy->setName(details.jointName);

        if (!details.addOffsetFrames)
        {
            copy->connectSocket_parent_frame(selectedPf);
            copy->connectSocket_child_frame(b);
        }
        else
        {
            // add first offset frame as joint's parent
            {
                auto pof1 = std::make_unique<OpenSim::PhysicalOffsetFrame>();
                pof1->setParentFrame(selectedPf);
                pof1->setName(selectedPf.getName() + "_offset");

                // care: ownership change happens here (#642)
                OpenSim::PhysicalOffsetFrame& ref = osc::AddFrame(*copy, std::move(pof1));
                copy->connectSocket_parent_frame(ref);
            }

            // add second offset frame as joint's child
            {
                auto pof2 = std::make_unique<OpenSim::PhysicalOffsetFrame>();
                pof2->setParentFrame(b);
                pof2->setName(b.getName() + "_offset");

                // care: ownership change happens here (#642)
                OpenSim::PhysicalOffsetFrame& ref = osc::AddFrame(*copy, std::move(pof2));
                copy->connectSocket_child_frame(ref);
            }
        }

        return copy;
    }

    bool TryReexpressComponentSpatialPropertiesInNewConnectee(
        OpenSim::Component& component,
        OpenSim::Object const& newConnectee,
        SimTK::State const& state)
    {
        auto const* const newFrame = dynamic_cast<OpenSim::Frame const*>(&newConnectee);
        if (!newFrame)
        {
            return false;  // new connectee isn't a frame
        }

        auto const spatialRep = osc::TryGetSpatialRepresentation(component, state);
        if (!spatialRep)
        {
            return false;  // cannot represent the component spatially
        }

        SimTK::Transform const currentParentToGround = spatialRep->parentToGround;
        SimTK::Transform const groundToNewConnectee = newFrame->getTransformInGround(state).invert();
        SimTK::Transform const currentParentToNewConnectee = groundToNewConnectee * currentParentToGround;

        if (auto* positionalProp = osc::FindSimplePropertyMut<SimTK::Vec3>(component, spatialRep->locationVec3PropertyName))
        {
            SimTK::Vec3 const oldPosition = positionalProp->getValue();
            SimTK::Vec3 const newPosition = currentParentToNewConnectee * oldPosition;

            positionalProp->setValue(newPosition);  // update property with new position
        }

        if (spatialRep->maybeOrientationVec3EulersPropertyName)
        {
            if (auto* orientationalProp = osc::FindSimplePropertyMut<SimTK::Vec3>(component, *spatialRep->maybeOrientationVec3EulersPropertyName))
            {
                SimTK::Rotation const currentRotationInGround = spatialRep->parentToGround.R();
                SimTK::Rotation const groundToNewConnecteeRotation = newFrame->getRotationInGround(state).invert();
                SimTK::Rotation const currentParentRotationToNewConnecteeRotation = groundToNewConnecteeRotation * currentRotationInGround;

                SimTK::Vec3 const oldEulers = orientationalProp->getValue();
                SimTK::Rotation const oldRotation = [oldEulers]()
                {
                    SimTK::Rotation rv;
                    rv.setRotationToBodyFixedXYZ(oldEulers);
                    return rv;
                }();
                SimTK::Rotation const newRotation = currentParentRotationToNewConnecteeRotation * oldRotation;
                SimTK::Vec3 const newEulers = newRotation.convertRotationToBodyFixedXYZ();

                orientationalProp->setValue(newEulers);
            }
        }

        return true;
    }
}

void osc::ActionSaveCurrentModelAs(UndoableModelStatePair& uim)
{
    std::optional<std::filesystem::path> const maybePath = PromptSaveOneFile();

    if (maybePath && TrySaveModel(uim.getModel(), maybePath->string()))
    {
        std::string const oldPath = uim.getModel().getInputFileName();

        uim.updModel().setInputFileName(maybePath->string());
        uim.setFilesystemPath(*maybePath);

        if (*maybePath != oldPath)
        {
            uim.commit("changed osim path");
        }
        uim.setUpToDateWithFilesystem(std::filesystem::last_write_time(*maybePath));

        App::upd().addRecentFile(*maybePath);
    }
}

void osc::ActionNewModel(ParentPtr<MainUIStateAPI> const& api)
{
    auto p = std::make_unique<UndoableModelStatePair>();
    api->addAndSelectTab<ModelEditorTab>(api, std::move(p));
}

void osc::ActionOpenModel(ParentPtr<MainUIStateAPI> const& api)
{
    DoOpenFileViaDialog(api);
}

void osc::ActionOpenModel(ParentPtr<MainUIStateAPI> const& api, std::filesystem::path const& path)
{
    OpenOsimInLoadingTab(api, path);
}

bool osc::ActionSaveModel(MainUIStateAPI&, UndoableModelStatePair& model)
{
    std::optional<std::string> const maybeUserSaveLoc = TryGetModelSaveLocation(model.getModel());

    if (maybeUserSaveLoc && TrySaveModel(model.getModel(), *maybeUserSaveLoc))
    {
        std::string const oldPath = model.getModel().getInputFileName();
        model.updModel().setInputFileName(*maybeUserSaveLoc);
        model.setFilesystemPath(*maybeUserSaveLoc);

        if (*maybeUserSaveLoc != oldPath)
        {
            model.commit("changed osim path");
        }
        model.setUpToDateWithFilesystem(std::filesystem::last_write_time(*maybeUserSaveLoc));

        App::upd().addRecentFile(*maybeUserSaveLoc);
        return true;
    }
    else
    {
        return false;
    }
}

void osc::ActionTryDeleteSelectionFromEditedModel(UndoableModelStatePair& uim)
{
    OpenSim::Component const* const selected = uim.getSelected();

    if (!selected)
    {
        return;
    }

    OpenSim::ComponentPath const selectedPath = osc::GetAbsolutePath(*selected);

    UID const oldVersion = uim.getModelVersion();
    OpenSim::Model& mutModel = uim.updModel();
    OpenSim::Component* const mutComponent = osc::FindComponentMut(mutModel, selectedPath);

    if (!mutComponent)
    {
        uim.setModelVersion(oldVersion);
        return;
    }

    std::string const selectedComponentName = mutComponent->getName();

    if (osc::TryDeleteComponentFromModel(mutModel, *mutComponent))
    {
        try
        {
            osc::InitializeModel(mutModel);
            osc::InitializeState(mutModel);

            std::stringstream ss;
            ss << "deleted " << selectedComponentName;
            uim.commit(std::move(ss).str());
        }
        catch (std::exception const& ex)
        {
            log::error("error detected while deleting a component: %s", ex.what());
            uim.rollback();
        }
    }
    else
    {
        uim.setModelVersion(oldVersion);
    }
}

void osc::ActionUndoCurrentlyEditedModel(UndoableModelStatePair& model)
{
    if (model.canUndo())
    {
        model.doUndo();
    }
}

void osc::ActionRedoCurrentlyEditedModel(UndoableModelStatePair& model)
{
    if (model.canRedo())
    {
        model.doRedo();
    }
}

void osc::ActionDisableAllWrappingSurfaces(UndoableModelStatePair& model)
{
    try
    {
        OpenSim::Model& mutModel = model.updModel();
        osc::DeactivateAllWrapObjectsIn(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        model.commit("disabled all wrapping surfaces");
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while disabling wrapping surfaces: %s", ex.what());
        model.rollback();
    }
}

void osc::ActionEnableAllWrappingSurfaces(UndoableModelStatePair& model)
{
    try
    {
        OpenSim::Model& mutModel = model.updModel();
        osc::ActivateAllWrapObjectsIn(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        model.commit("enabled all wrapping surfaces");
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while enabling wrapping surfaces: %s", ex.what());
        model.rollback();
    }
}

void osc::ActionClearSelectionFromEditedModel(UndoableModelStatePair& model)
{
    model.setSelected(nullptr);
}

bool osc::ActionLoadSTOFileAgainstModel(
    ParentPtr<MainUIStateAPI> const& parent,
    UndoableModelStatePair const& uim,
    std::filesystem::path const& stoPath)
{
    try
    {
        auto modelCopy = std::make_unique<OpenSim::Model>(uim.getModel());
        osc::InitializeModel(*modelCopy);
        osc::InitializeState(*modelCopy);

        auto simulation = std::make_shared<Simulation>(StoFileSimulation{std::move(modelCopy), stoPath, uim.getFixupScaleFactor()});

        parent->addAndSelectTab<SimulatorTab>(parent, simulation);

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to load an STO file against the model: %s", ex.what());
        return false;
    }
}

bool osc::ActionStartSimulatingModel(
    ParentPtr<MainUIStateAPI> const& parent,
    UndoableModelStatePair const& uim)
{
    BasicModelStatePair modelState{uim};
    ForwardDynamicSimulatorParams params = osc::FromParamBlock(parent->getSimulationParams());

    auto simulation = std::make_shared<Simulation>(ForwardDynamicSimulation{std::move(modelState), params});
    auto simulationTab = std::make_unique<SimulatorTab>(parent, std::move(simulation));

    parent->selectTab(parent->addTab(std::move(simulationTab)));

    return true;
}

bool osc::ActionUpdateModelFromBackingFile(UndoableModelStatePair& uim)
{
    if (!uim.hasFilesystemLocation())
    {
        // there is no backing file?
        return false;
    }

    std::filesystem::path const path = uim.getFilesystemPath();

    if (!std::filesystem::exists(path))
    {
        // the file does not exist? (e.g. because the user deleted it externally - #495)
        return false;
    }

    std::filesystem::file_time_type const lastSaveTime = std::filesystem::last_write_time(path);

    if (uim.getLastFilesystemWriteTime() >= lastSaveTime)
    {
        // the backing file is probably up-to-date with the in-memory representation
        //
        // (e.g. because OSC just saved it and set the timestamp appropriately)
        return false;
    }

    // else: there is a backing file and it's newer than what's in-memory, so reload
    try
    {
        log::info("file change detected: loading updated file");

        auto loadedModel = std::make_unique<OpenSim::Model>(uim.getModel().getInputFileName());

        log::info("loaded updated file");

        uim.setModel(std::move(loadedModel));
        uim.commit("reloaded osim");
        uim.setUpToDateWithFilesystem(lastSaveTime);

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to automatically load a model file: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionCopyModelPathToClipboard(UndoableModelStatePair const& uim)
{
    if (!uim.hasFilesystemLocation())
    {
        // there is no backing file?
        return false;
    }

    std::filesystem::path const absPath = std::filesystem::absolute(uim.getFilesystemPath());

    osc::SetClipboardText(absPath.string().c_str());

    return true;
}

bool osc::ActionAutoscaleSceneScaleFactor(UndoableModelStatePair& uim)
{
    float const sf = osc::GetRecommendedScaleFactor(
        *osc::App::singleton<osc::MeshCache>(),
        uim.getModel(),
        uim.getState(),
        OpenSimDecorationOptions{}
    );
    uim.setFixupScaleFactor(sf);
    return true;
}

bool osc::ActionToggleFrames(UndoableModelStatePair& uim)
{
    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        bool const newState = osc::ToggleShowingFrames(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit(newState ? "shown frames" : "hidden frames");

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to toggle frames: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionToggleMarkers(UndoableModelStatePair& uim)
{
    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        bool const newState = osc::ToggleShowingMarkers(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit(newState ? "shown markers" : "hidden markers");

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to toggle markers: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionToggleContactGeometry(UndoableModelStatePair& uim)
{
    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        bool const newState = osc::ToggleShowingContactGeometry(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit(newState ? "shown contact geometry" : "hidden contact geometry");

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to toggle contact geometry: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionToggleWrapGeometry(UndoableModelStatePair& uim)
{
    try
    {
        OpenSim::Model& mutModel = uim.updModel();
        bool const newState = osc::ToggleShowingWrapGeometry(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit(newState ? "shown wrap geometry" : "hidden wrap geometry");

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to toggle wrap geometry: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionOpenOsimParentDirectory(UndoableModelStatePair& uim)
{
    if (!osc::HasInputFileName(uim.getModel()))
    {
        return false;
    }

    std::filesystem::path const p{uim.getModel().getInputFileName()};
    osc::OpenPathInOSDefaultApplication(p.parent_path());
    return true;
}

bool osc::ActionOpenOsimInExternalEditor(UndoableModelStatePair& uim)
{
    if (!osc::HasInputFileName(uim.getModel()))
    {
        return false;
    }

    osc::OpenPathInOSDefaultApplication(uim.getModel().getInputFileName());
    return true;
}

bool osc::ActionReloadOsimFromDisk(UndoableModelStatePair& uim, MeshCache& meshCache)
{
    if (!osc::HasInputFileName(uim.getModel()))
    {
        log::error("cannot reload the osim file: the model doesn't appear to have a backing file (is it saved?)");
        return false;
    }

    try
    {
        log::info("manual osim file reload requested: attempting to reload the file");
        auto p = std::make_unique<OpenSim::Model>(uim.getModel().getInputFileName());
        log::info("loaded updated file");

        uim.setModel(std::move(p));
        uim.commit("reloaded from filesystem");
        uim.setUpToDateWithFilesystem(std::filesystem::last_write_time(uim.getFilesystemPath()));

        // #594: purge the app-wide mesh cache so that any user edits to the underlying
        // mesh files are immediately visible after reloading
        //
        // this is useful for users that are actively editing the meshes of the model file
        meshCache.clear();

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to reload a model file: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionSimulateAgainstAllIntegrators(
    ParentPtr<MainUIStateAPI> const& parent,
    UndoableModelStatePair const& uim)
{
    parent->addAndSelectTab<PerformanceAnalyzerTab>(
        parent,
        BasicModelStatePair{uim},
        parent->getSimulationParams()
    );
    return true;
}

bool osc::ActionAddOffsetFrameToPhysicalFrame(UndoableModelStatePair& uim, OpenSim::ComponentPath const& path)
{
    auto const* const target = osc::FindComponent<OpenSim::PhysicalFrame>(uim.getModel(), path);
    if (!target)
    {
        return false;
    }

    std::string const newPofName = target->getName() + "_offsetframe";

    auto pof = std::make_unique<OpenSim::PhysicalOffsetFrame>();
    pof->setName(newPofName);
    pof->setParentFrame(*target);

    UID const oldVersion = uim.getModelVersion();  // for rollbacks
    try
    {
        OpenSim::Model& mutModel = uim.updModel();

        auto* const mutTarget = osc::FindComponentMut<OpenSim::PhysicalFrame>(mutModel, path);
        if (!mutTarget)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        OpenSim::PhysicalOffsetFrame& pofRef = osc::AddComponent(*mutTarget, std::move(pof));
        osc::FinalizeConnections(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.setSelected(&pofRef);
        uim.commit("added " + newPofName);

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to add a frame to %s: %s", path.toString().c_str(), ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::CanRezeroJoint(UndoableModelStatePair& uim, OpenSim::ComponentPath const& jointPath)
{
    auto const* const joint = osc::FindComponent<OpenSim::Joint>(uim.getModel(), jointPath);
    if (!joint)
    {
        return false;
    }

    // if the joint uses offset frames for both its parent and child frames then
    // it is possible to reorient those frames such that the joint's new zero
    // point is whatever the current arrangement is (effectively, by pre-transforming
    // the parent into the child and assuming a "zeroed" joint is an identity op)

    return dynamic_cast<OpenSim::PhysicalOffsetFrame const*>(&joint->getParentFrame()) != nullptr;
}

bool osc::ActionRezeroJoint(UndoableModelStatePair& uim, OpenSim::ComponentPath const& jointPath)
{
    auto const* const target = osc::FindComponent<OpenSim::Joint>(uim.getModel(), jointPath);
    if (!target)
    {
        return false;  // nothing/invalid component type specified
    }

    auto const* const parentPOF = dynamic_cast<OpenSim::PhysicalOffsetFrame const*>(&target->getParentFrame());
    if (!parentPOF)
    {
        return false;  // target has no parent frame
    }

    OpenSim::ComponentPath const parentPath = osc::GetAbsolutePath(*parentPOF);
    OpenSim::PhysicalFrame const& childFrame = target->getChildFrame();
    SimTK::Transform const parentXform = parentPOF->getTransformInGround(uim.getState());
    SimTK::Transform const childXform = childFrame.getTransformInGround(uim.getState());
    SimTK::Transform const child2parent = parentXform.invert() * childXform;
    SimTK::Transform const newXform = parentPOF->getOffsetTransform() * child2parent;

    UID const oldVersion = uim.getModelVersion();  // for rollbacks
    try
    {
        OpenSim::Model& mutModel = uim.updModel();

        auto* const mutJoint = osc::FindComponentMut<OpenSim::Joint>(mutModel, jointPath);
        if (!mutJoint)
        {
            uim.setModelVersion(oldVersion);  // cannot find mutable version of the joint
            return false;
        }

        auto* const mutParent = osc::FindComponentMut<OpenSim::PhysicalOffsetFrame>(mutModel, parentPath);
        if (!mutParent)
        {
            uim.setModelVersion(oldVersion);  // cannot find mutable version of the parent offset frame
            return false;
        }

        // else: perform model transformation

        std::string const jointName = mutJoint->getName();

        // first, zero all the joint's coordinates
        //
        // (we're assuming that the new transform performs the same function)
        for (int i = 0, nc = mutJoint->getProperty_coordinates().size(); i < nc; ++i)
        {
            mutJoint->upd_coordinates(i).setDefaultValue(0.0);
        }

        // then set the parent offset frame's transform to "do the work"
        mutParent->setOffsetTransform(newXform);

        // and then put the model back into a valid state, ready for committing etc.
        osc::FinalizeConnections(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit("rezeroed " + jointName);

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to rezero a joint: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAddParentOffsetFrameToJoint(UndoableModelStatePair& uim, OpenSim::ComponentPath const& jointPath)
{
    auto const* const target = osc::FindComponent<OpenSim::Joint>(uim.getModel(), jointPath);
    if (!target)
    {
        return false;
    }

    auto pf = std::make_unique<OpenSim::PhysicalOffsetFrame>();
    pf->setParentFrame(target->getParentFrame());

    UID const oldVersion = uim.getModelVersion();  // for rollbacks
    try
    {
        OpenSim::Model& mutModel = uim.updModel();

        auto* const mutJoint = osc::FindComponentMut<OpenSim::Joint>(mutModel, jointPath);
        if (!mutJoint)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        std::string const jointName = mutJoint->getName();

        osc::AddFrame(*mutJoint, std::move(pf));
        osc::FinalizeConnections(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit("added " + jointName);

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to add a parent offset frame: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAddChildOffsetFrameToJoint(UndoableModelStatePair& uim, OpenSim::ComponentPath const& jointPath)
{
    auto const* const target = osc::FindComponent<OpenSim::Joint>(uim.getModel(), jointPath);
    if (!target)
    {
        return false;
    }

    auto pf = std::make_unique<OpenSim::PhysicalOffsetFrame>();
    pf->setParentFrame(target->getChildFrame());

    UID const oldVersion = uim.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = uim.updModel();

        auto* const mutJoint = osc::FindComponentMut<OpenSim::Joint>(mutModel, jointPath);
        if (!mutJoint)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        std::string const jointName = mutJoint->getName();

        osc::AddFrame(*mutJoint, std::move(pf));
        osc::FinalizeConnections(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit("added " + jointName);

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to add a child offset frame: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionSetComponentName(UndoableModelStatePair& uim, OpenSim::ComponentPath const& path, std::string const& newName)
{
    if (newName.empty())
    {
        return false;
    }

    OpenSim::Component const* const target = osc::FindComponent(uim.getModel(), path);
    if (!target)
    {
        return false;
    }

    UID const oldVersion = uim.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = uim.updModel();

        OpenSim::Component* const mutComponent = osc::FindComponentMut(mutModel, path);
        if (!mutComponent)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        std::string const oldName = mutComponent->getName();
        mutComponent->setName(newName);
        osc::FinalizeConnections(mutModel);  // because pointers need to know the new name
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.setSelected(mutComponent);  // because the name changed

        std::stringstream ss;
        ss << "renamed " << oldName << " to " << newName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to set a component's name: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionChangeJointTypeTo(UndoableModelStatePair& uim, OpenSim::ComponentPath const& jointPath, std::unique_ptr<OpenSim::Joint> newType)
{
    if (!newType)
    {
        log::error("new joint type provided to ChangeJointType function is nullptr: cannot continue: this is a developer error and should be reported");
        return false;
    }

    auto const* const target = osc::FindComponent<OpenSim::Joint>(uim.getModel(), jointPath);
    if (!target)
    {
        return false;
    }

    auto const* const owner = osc::GetOwner<OpenSim::JointSet>(*target);
    if (!owner)
    {
        return false;
    }

    OpenSim::ComponentPath const ownerPath = osc::GetAbsolutePath(*owner);

    std::optional<size_t> const maybeIdx = FindJointInParentJointSet(*target);
    if (!maybeIdx)
    {
        return false;
    }

    size_t const idx = *maybeIdx;

    std::string const oldTypeName = target->getConcreteClassName();
    std::string const newTypeName = newType->getConcreteClassName();

    osc::CopyCommonJointProperties(*target, *newType);

    // perform model update by overwriting the old joint in model
    //
    // note: this will invalidate the input joint, because the
    // OpenSim::JointSet container will automatically kill it

    UID const oldVersion = uim.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = uim.updModel();

        auto* const mutParent = osc::FindComponentMut<OpenSim::JointSet>(mutModel, ownerPath);
        if (!mutParent)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        OpenSim::Joint const& jointRef = osc::Assign(*mutParent, idx, std::move(newType));
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.setSelected(&jointRef);

        std::stringstream ss;
        ss << "changed " << oldTypeName << " to " << newTypeName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to change a joint's type: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAttachGeometryToPhysicalFrame(UndoableModelStatePair& uim, OpenSim::ComponentPath const& physFramePath, std::unique_ptr<OpenSim::Geometry> geom)
{
    auto const* const target = osc::FindComponent<OpenSim::PhysicalFrame>(uim.getModel(), physFramePath);
    if (!target)
    {
        return false;
    }

    UID const oldVersion = uim.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = uim.updModel();

        auto* const mutPof = osc::FindComponentMut<OpenSim::PhysicalFrame>(mutModel, physFramePath);
        if (!mutPof)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        std::string const pofName = mutPof->getName();

        osc::AttachGeometry(*mutPof, std::move(geom));
        osc::FinalizeConnections(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "attached geometry to " << pofName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to attach geometry to the a physical frame: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAssignContactGeometryToHCF(
    UndoableModelStatePair& uim,
    OpenSim::ComponentPath const& hcfPath,
    OpenSim::ComponentPath const& contactGeomPath)
{
    auto const* const target = osc::FindComponent<OpenSim::HuntCrossleyForce>(uim.getModel(), hcfPath);
    if (!target)
    {
        return false;
    }

    auto const* const geom = osc::FindComponent<OpenSim::ContactGeometry>(uim.getModel(), contactGeomPath);
    if (!geom)
    {
        return false;
    }

    UID const oldVersion = uim.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = uim.updModel();

        auto* const mutHCF = osc::FindComponentMut<OpenSim::HuntCrossleyForce>(mutModel, hcfPath);
        if (!mutHCF)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        // calling this ensures at least one `OpenSim::HuntCrossleyForce::ContactParameters`
        // is present in the HCF
        mutHCF->getStaticFriction();
        OSC_ASSERT(!osc::empty(mutHCF->updContactParametersSet()));

        mutHCF->updContactParametersSet()[0].updGeometry().appendValue(geom->getName());
        osc::FinalizeConnections(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit("added contact geometry");

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to assign contact geometry to a HCF: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionApplyPropertyEdit(UndoableModelStatePair& uim, ObjectPropertyEdit& resp)
{
    UID const oldVersion = uim.getModelVersion();
    try
    {
        OpenSim::Model& model = uim.updModel();

        OpenSim::Component* const component = osc::FindComponentMut(model, resp.getComponentAbsPath());
        if (!component)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        OpenSim::AbstractProperty* const prop = osc::FindPropertyMut(*component, resp.getPropertyName());
        if (!prop)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        std::string const propName = prop->getName();

        resp.apply(*prop);

        std::string const newValue = prop->toStringForDisplay(3);

        osc::InitializeModel(model);
        osc::InitializeState(model);

        std::stringstream ss;
        ss << "set " << propName << " to " << newValue;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to apply a property edit: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAddPathPointToPathActuator(
    UndoableModelStatePair& uim,
    OpenSim::ComponentPath const& pathActuatorPath,
    OpenSim::ComponentPath const& pointPhysFrame)
{
    auto const* const pa = osc::FindComponent<OpenSim::PathActuator>(uim.getModel(), pathActuatorPath);
    if (!pa)
    {
        return false;
    }

    auto const* const pf = osc::FindComponent<OpenSim::PhysicalFrame>(uim.getModel(), pointPhysFrame);
    if (!pf)
    {
        return false;
    }

    size_t const n = osc::size(pa->getGeometryPath().getPathPointSet());
    std::string const name = pa->getName() + "-P" + std::to_string(n + 1);
    SimTK::Vec3 const pos = {0.0f, 0.0f, 0.0f};

    UID const oldVersion = uim.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = uim.updModel();

        auto* const mutPA = osc::FindComponentMut<OpenSim::PathActuator>(mutModel, pathActuatorPath);
        if (!mutPA)
        {
            uim.setModelVersion(oldVersion);
            return false;
        }

        std::string const paName = mutPA->getName();

        mutPA->addNewPathPoint(name, *pf, pos);
        osc::FinalizeConnections(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        // try to select the new path point, if possible, so that the user
        // can immediately see the grab handles etc. (#779)
        if (auto const* paAfterFinalization = FindComponent<OpenSim::PathActuator>(mutModel, pathActuatorPath))
        {
            auto const& pps = paAfterFinalization->getGeometryPath().getPathPointSet();
            if (!empty(pps))
            {
                uim.setSelected(&At(pps, ssize(pps) -1));
            }
        }

        std::stringstream ss;
        ss << "added path point to " << paName;
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to add a path point to a path actuator: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionReassignComponentSocket(
    UndoableModelStatePair& uim,
    OpenSim::ComponentPath const& componentAbsPath,
    std::string const& socketName,
    OpenSim::Object const& connectee,
    SocketReassignmentFlags flags,
    std::string& error)
{
    // HOTFIX for #382
    //
    // OpenSim can segfault if certain types of circular joint connections to `/ground` are made.
    // This early-out error just ensures that OpenSim Creator isn't nuked by that OpenSim bug
    //
    // issue #3299 in opensim-core
    if (socketName == "child_frame" && &connectee == &uim.getModel().getGround())
    {
        error = "Error: you cannot assign a joint's child frame to ground: this is a known bug in OpenSim (see issue #382 in ComputationalBiomechanicsLab/opensim-creator and issue #3299 in opensim-org/opensim-core)";
        return false;
    }

    OpenSim::Component const* const target = osc::FindComponent(uim.getModel(), componentAbsPath);
    if (!target)
    {
        return false;
    }

    UID const oldVersion = uim.getModelVersion();

    OpenSim::Model& mutModel = uim.updModel();

    OpenSim::Component* const mutComponent = osc::FindComponentMut(mutModel, componentAbsPath);
    if (!mutComponent)
    {
        uim.setModelVersion(oldVersion);
        return false;
    }

    OpenSim::AbstractSocket* const mutSocket = osc::FindSocketMut(*mutComponent, socketName);
    if (!mutSocket)
    {
        uim.setModelVersion(oldVersion);
        return false;
    }

    try
    {
        bool const componentPropertiesReexpressed = flags & SocketReassignmentFlags::TryReexpressComponentInNewConnectee ?
            TryReexpressComponentSpatialPropertiesInNewConnectee(*mutComponent, connectee, uim.getState()) :
            false;

        if (componentPropertiesReexpressed)
        {
            osc::FinalizeFromProperties(mutModel);
        }
        mutSocket->connect(connectee);
        osc::FinalizeConnections(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.commit("reassigned socket");

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to reassign a socket: %s", ex.what());
        error = ex.what();
        uim.rollback();
        return false;
    }
}

bool osc::ActionSetModelSceneScaleFactorTo(UndoableModelStatePair& uim, float v)
{
    uim.setFixupScaleFactor(v);
    return true;
}

osc::BodyDetails::BodyDetails() :
    centerOfMass{0.0f, 0.0f, 0.0f},
    inertia{1.0f, 1.0f, 1.0f},
    mass{1.0f},
    bodyName{"new_body"},
    jointTypeIndex{osc::IndexOf<OpenSim::WeldJoint>(osc::GetComponentRegistry<OpenSim::Joint>()).value_or(0)},
    maybeGeometry{nullptr},
    addOffsetFrames{true}
{
}

bool osc::ActionAddBodyToModel(UndoableModelStatePair& uim, BodyDetails const& details)
{
    auto const* const parent = osc::FindComponent<OpenSim::PhysicalFrame>(uim.getModel(), details.parentFrameAbsPath);
    if (!parent)
    {
        return false;
    }

    SimTK::Vec3 const com = ToSimTKVec3(details.centerOfMass);
    SimTK::Inertia const inertia = ToSimTKInertia(details.inertia);
    auto const mass = static_cast<double>(details.mass);

    // create body
    auto body = std::make_unique<OpenSim::Body>(details.bodyName, mass, com, inertia);

    // create joint between body and whatever the frame is
    OpenSim::Joint const& jointProto = osc::At(osc::GetComponentRegistry<OpenSim::Joint>(), details.jointTypeIndex).prototype();
    std::unique_ptr<OpenSim::Joint> joint = MakeJoint(details, *body, jointProto, *parent);

    // attach decorative geom
    if (details.maybeGeometry)
    {
        osc::AttachGeometry(*body, osc::Clone(*details.maybeGeometry));
    }

    // mutate the model and perform the edit
    try
    {
        OpenSim::Model& mutModel = uim.updModel();

        osc::AddJoint(mutModel, std::move(joint));
        OpenSim::Body& bodyRef = osc::AddBody(mutModel, std::move(body));
        osc::FinalizeConnections(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        uim.setSelected(&bodyRef);

        std::stringstream ss;
        ss << "added " << bodyRef.getName();
        uim.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to add a body to the model: %s", ex.what());
        uim.rollback();
        return false;
    }
}

bool osc::ActionAddComponentToModel(UndoableModelStatePair& model, std::unique_ptr<OpenSim::Component> c, std::string& errorOut)
{
    if (c == nullptr)
    {
        return false;  // paranoia
    }

    try
    {
        OpenSim::Model& mutModel = model.updModel();

        OpenSim::Component const& ref = AddComponentToAppropriateSet(mutModel, std::move(c));
        osc::FinalizeConnections(mutModel);
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);
        model.setSelected(&ref);

        std::stringstream ss;
        ss << "added " << ref.getName();
        model.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        errorOut = ex.what();
        log::error("error detected while trying to add a component to the model: %s", ex.what());
        model.rollback();
        return false;
    }
}

bool osc::ActionSetCoordinateSpeed(
    UndoableModelStatePair& model,
    OpenSim::Coordinate const& coord,
    double newSpeed)
{
    OpenSim::ComponentPath const coordPath = osc::GetAbsolutePath(coord);

    UID const oldVersion = model.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = model.updModel();

        auto* const mutCoord = osc::FindComponentMut<OpenSim::Coordinate>(mutModel, coordPath);
        if (!mutCoord)
        {
            model.setModelVersion(oldVersion);  // can't find the coordinate within the provided model
            return false;
        }

        // HACK: don't do a full model+state re-realization here: only do it
        //       when the caller wants to save the coordinate change
        mutCoord->setDefaultSpeedValue(newSpeed);
        mutCoord->setSpeedValue(mutModel.updWorkingState(), newSpeed);
        mutModel.equilibrateMuscles(mutModel.updWorkingState());
        mutModel.realizeDynamics(mutModel.updWorkingState());

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to set a coordinate's speed: %s", ex.what());
        model.rollback();
        return false;
    }
}

bool osc::ActionSetCoordinateSpeedAndSave(
    UndoableModelStatePair& model,
    OpenSim::Coordinate const& coord,
    double newSpeed)
{
    if (ActionSetCoordinateSpeed(model, coord, newSpeed))
    {
        OpenSim::Model& mutModel = model.updModel();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "set " << coord.getName() << "'s speed";
        model.commit(std::move(ss).str());

        return true;
    }
    else
    {
        // edit wasn't made
        return false;
    }
}

bool osc::ActionSetCoordinateLockedAndSave(UndoableModelStatePair& model, OpenSim::Coordinate const& coord, bool v)
{
    OpenSim::ComponentPath const coordPath = osc::GetAbsolutePath(coord);

    UID const oldVersion = model.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = model.updModel();

        auto* const mutCoord = osc::FindComponentMut<OpenSim::Coordinate>(mutModel, coordPath);
        if (!mutCoord)
        {
            model.setModelVersion(oldVersion);  // can't find the coordinate within the provided model
            return false;
        }

        mutCoord->setDefaultLocked(v);
        mutCoord->setLocked(mutModel.updWorkingState(), v);
        mutModel.equilibrateMuscles(mutModel.updWorkingState());
        mutModel.realizeDynamics(mutModel.updWorkingState());

        std::stringstream ss;
        ss << (v ? "locked " : "unlocked ") << mutCoord->getName();
        model.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to lock a coordinate: %s", ex.what());
        model.rollback();
        return false;
    }
}

// set the value of a coordinate, but don't save it to the model (yet)
bool osc::ActionSetCoordinateValue(
    UndoableModelStatePair& model,
    OpenSim::Coordinate const& coord,
    double newValue)
{
    OpenSim::ComponentPath const coordPath = osc::GetAbsolutePath(coord);

    UID const oldVersion = model.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = model.updModel();

        auto* const mutCoord = osc::FindComponentMut<OpenSim::Coordinate>(mutModel, coordPath);
        if (!mutCoord)
        {
            model.setModelVersion(oldVersion);  // can't find the coordinate within the provided model
            return false;
        }

        double const rangeMin = std::min(mutCoord->getRangeMin(), mutCoord->getRangeMax());
        double const rangeMax = std::max(mutCoord->getRangeMin(), mutCoord->getRangeMax());

        if (!(rangeMin <= newValue && newValue <= rangeMax))
        {
            model.setModelVersion(oldVersion);  // the requested edit is outside the coordinate's allowed range
            return false;
        }

        // HACK: don't do a full model+state re-realization here: only do it
        //       when the caller wants to save the coordinate change
        mutCoord->setDefaultValue(newValue);
        mutCoord->setValue(mutModel.updWorkingState(), newValue);
        mutModel.equilibrateMuscles(mutModel.updWorkingState());
        mutModel.realizeDynamics(mutModel.updWorkingState());

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to set a coordinate's value: %s", ex.what());
        model.rollback();
        return false;
    }
}

// set the value of a coordinate and ensure it is saved into the model
bool osc::ActionSetCoordinateValueAndSave(
    UndoableModelStatePair& model,
    OpenSim::Coordinate const& coord,
    double newValue)
{
    if (ActionSetCoordinateValue(model, coord, newValue))
    {
        OpenSim::Model& mutModel = model.updModel();

        // CAREFUL: ensure that *all* coordinate's default values are updated to reflect
        //          the current state
        //
        // You might be thinking "but, the caller only wanted to set one coordinate". You're
        // right, but OpenSim models can contain constraints where editing one coordinate causes
        // a bunch of other coordinates to change.
        //
        // See #345 for a longer explanation
        for (OpenSim::Coordinate& c : mutModel.updComponentList<OpenSim::Coordinate>())
        {
            c.setDefaultValue(c.getValue(model.getState()));
        }

        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "set " << coord.getName() << " to " << osc::ConvertCoordValueToDisplayValue(coord, newValue);
        model.commit(std::move(ss).str());

        return true;
    }
    else
    {
        return false;  // an edit wasn't made
    }
}

bool osc::ActionSetComponentAndAllChildrensIsVisibleTo(
    UndoableModelStatePair& model,
    OpenSim::ComponentPath const& path,
    bool newVisibility)
{
    UID const oldVersion = model.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = model.updModel();

        OpenSim::Component* const mutComponent = osc::FindComponentMut(mutModel, path);
        if (!mutComponent)
        {
            model.setModelVersion(oldVersion);  // can't find the coordinate within the provided model
            return false;
        }

        TrySetAppearancePropertyIsVisibleTo(*mutComponent, newVisibility);

        for (OpenSim::Component& c : mutComponent->updComponentList())
        {
            TrySetAppearancePropertyIsVisibleTo(c, newVisibility);
        }

        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "set " << path.getComponentName() << " visibility to " << newVisibility;
        model.commit(std::move(ss).str());

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to hide a component: %s", ex.what());
        model.rollback();
        return false;
    }
}

bool osc::ActionShowOnlyComponentAndAllChildren(UndoableModelStatePair& model, OpenSim::ComponentPath const& path)
{
    UID const oldVersion = model.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = model.updModel();

        OpenSim::Component* const mutComponent = osc::FindComponentMut(mutModel, path);
        if (!mutComponent)
        {
            model.setModelVersion(oldVersion);  // can't find the coordinate within the provided model
            return false;
        }

        // first, hide everything in the model
        for (OpenSim::Component& c : mutModel.updComponentList())
        {
            TrySetAppearancePropertyIsVisibleTo(c, false);
        }

        // then show the intended component and its children
        TrySetAppearancePropertyIsVisibleTo(*mutComponent, true);
        for (OpenSim::Component& c : mutComponent->updComponentList())
        {
            TrySetAppearancePropertyIsVisibleTo(c, true);
        }

        // reinitialize etc.
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        // commit it
        {
            std::stringstream ss;
            ss << "showing only " << path.getComponentName();
            model.commit(std::move(ss).str());
        }

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to hide a component: %s", ex.what());
        model.rollback();
        return false;
    }
}

bool osc::ActionSetComponentAndAllChildrenWithGivenConcreteClassNameIsVisibleTo(
    UndoableModelStatePair& model,
    OpenSim::ComponentPath const& root,
    std::string_view concreteClassName,
    bool newVisibility)
{
    UID const oldVersion = model.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = model.updModel();

        OpenSim::Component* const mutComponent = osc::FindComponentMut(mutModel, root);
        if (!mutComponent)
        {
            model.setModelVersion(oldVersion);  // can't find the coordinate within the provided model
            return false;
        }

        // first, hide everything in the model
        for (OpenSim::Component& c : mutModel.updComponentList())
        {
            if (c.getConcreteClassName() == concreteClassName)
            {
                TrySetAppearancePropertyIsVisibleTo(c, newVisibility);
                for (OpenSim::Component& child : c.updComponentList())
                {
                    TrySetAppearancePropertyIsVisibleTo(child, newVisibility);
                }
            }
        }

        // reinitialize etc.
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        // commit it
        {
            std::stringstream ss;
            if (newVisibility)
            {
                ss << "showing ";
            }
            else
            {
                ss << "hiding ";
            }
            ss << concreteClassName;
            model.commit(std::move(ss).str());
        }

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to show/hide components of a given type: %s", ex.what());
        model.rollback();
        return false;
    }
}

bool osc::ActionTranslateStation(
    UndoableModelStatePair& model,
    OpenSim::Station const& station,
    glm::vec3 const& deltaPosition)
{
    OpenSim::ComponentPath const stationPath = osc::GetAbsolutePath(station);
    UID const oldVersion = model.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = model.updModel();

        auto* const mutStation = FindComponentMut<OpenSim::Station>(mutModel, stationPath);
        if (!mutStation)
        {
            model.setModelVersion(oldVersion);  // the provided path isn't a station
            return false;
        }

        SimTK::Vec3 const originalPos = mutStation->get_location();
        SimTK::Vec3 const newPos = originalPos + ToSimTKVec3(deltaPosition);

        // perform mutation
        mutStation->set_location(newPos);

        // HACK: don't perform a full model reinitialization because that would be very expensive
        // and it is very likely that it isn't necessary when dragging a station
        //
        // osc::InitializeModel(mutModel);  // don't do this
        osc::InitializeState(mutModel);

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to move a station: %s", ex.what());
        model.rollback();
        return false;
    }
}

bool osc::ActionTranslateStationAndSave(
    UndoableModelStatePair& model,
    OpenSim::Station const& station,
    glm::vec3 const& deltaPosition)
{
    if (ActionTranslateStation(model, station, deltaPosition))
    {
        OpenSim::Model& mutModel = model.updModel();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "translated " << station.getName();
        model.commit(std::move(ss).str());

        return true;
    }
    else
    {
        return false;  // edit wasn't made
    }
}

bool osc::ActionTranslatePathPoint(
    UndoableModelStatePair& model,
    OpenSim::PathPoint const& pathPoint,
    glm::vec3 const& deltaPosition)
{
    OpenSim::ComponentPath const ppPath = osc::GetAbsolutePath(pathPoint);
    UID const oldVersion = model.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = model.updModel();

        auto* const mutPathPoint = FindComponentMut<OpenSim::PathPoint>(mutModel, ppPath);
        if (!mutPathPoint)
        {
            model.setModelVersion(oldVersion);  // the provided path isn't a station
            return false;
        }

        SimTK::Vec3 const originalPos = mutPathPoint->get_location();
        SimTK::Vec3 const newPos = originalPos + ToSimTKVec3(deltaPosition);

        // perform mutation
        mutPathPoint->setLocation(newPos);
        osc::InitializeState(mutModel);

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to move a station: %s", ex.what());
        model.rollback();
        return false;
    }
}

bool osc::ActionTranslatePathPointAndSave(
    UndoableModelStatePair& model,
    OpenSim::PathPoint const& pathPoint,
    glm::vec3 const& deltaPosition)
{
    if (ActionTranslatePathPoint(model, pathPoint, deltaPosition))
    {
        OpenSim::Model& mutModel = model.updModel();
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        std::stringstream ss;
        ss << "translated " << pathPoint.getName();
        model.commit(std::move(ss).str());

        return true;
    }
    else
    {
        return false;  // edit wasn't made
    }
}

bool osc::ActionTransformPof(
    UndoableModelStatePair& model,
    OpenSim::PhysicalOffsetFrame const& pof,
    glm::vec3 const& deltaTranslationInParentFrame,
    glm::vec3 const& newPofEulers)
{
    OpenSim::ComponentPath const pofPath = osc::GetAbsolutePath(pof);
    UID const oldVersion = model.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = model.updModel();

        auto* const mutPof = FindComponentMut<OpenSim::PhysicalOffsetFrame>(mutModel, pofPath);
        if (!mutPof)
        {
            model.setModelVersion(oldVersion);  // the provided path isn't a station
            return false;
        }

        SimTK::Vec3 const originalPos = mutPof->get_translation();
        SimTK::Vec3 const newPos = originalPos + ToSimTKVec3(deltaTranslationInParentFrame);

        // perform mutation
        mutPof->set_translation(newPos);
        mutPof->set_orientation(ToSimTKVec3(newPofEulers));
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to transform a POF: %s", ex.what());
        model.rollback();
        return false;
    }
    return false;
}

bool osc::ActionTransformWrapObject(
    UndoableModelStatePair& model,
    OpenSim::WrapObject const& wo,
    glm::vec3 const& deltaPosition,
    glm::vec3 const& newEulers)
{
    OpenSim::ComponentPath const pofPath = osc::GetAbsolutePath(wo);
    UID const oldVersion = model.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = model.updModel();

        auto* const mutPof = FindComponentMut<OpenSim::WrapObject>(mutModel, pofPath);
        if (!mutPof)
        {
            model.setModelVersion(oldVersion);  // the provided path isn't a station
            return false;
        }

        SimTK::Vec3 const originalPos = mutPof->get_translation();
        SimTK::Vec3 const newPos = originalPos + ToSimTKVec3(deltaPosition);

        // perform mutation
        mutPof->set_translation(newPos);
        mutPof->set_xyz_body_rotation(ToSimTKVec3(newEulers));
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to transform a POF: %s", ex.what());
        model.rollback();
        return false;
    }
    return false;
}

bool osc::ActionTransformContactGeometry(
    UndoableModelStatePair& model,
    OpenSim::ContactGeometry const& contactGeom,
    glm::vec3 const& deltaPosition,
    glm::vec3 const& newEulers)
{
    OpenSim::ComponentPath const pofPath = osc::GetAbsolutePath(contactGeom);
    UID const oldVersion = model.getModelVersion();
    try
    {
        OpenSim::Model& mutModel = model.updModel();

        auto* const mutGeom = FindComponentMut<OpenSim::ContactGeometry>(mutModel, pofPath);
        if (!mutGeom)
        {
            model.setModelVersion(oldVersion);  // the provided path doesn't exist in the model
            return false;
        }

        SimTK::Vec3 const originalPos = mutGeom->get_location();
        SimTK::Vec3 const newPos = originalPos + ToSimTKVec3(deltaPosition);

        // perform mutation
        mutGeom->set_location(newPos);
        mutGeom->set_orientation(ToSimTKVec3(newEulers));
        osc::InitializeModel(mutModel);
        osc::InitializeState(mutModel);

        return true;
    }
    catch (std::exception const& ex)
    {
        log::error("error detected while trying to transform a POF: %s", ex.what());
        model.rollback();
        return false;
    }
    return false;
}
