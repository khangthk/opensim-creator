#include "ParamBlockEditorPopup.hpp"

#include "osc_config.hpp"

#include "src/Bindings/ImGuiHelpers.hpp"
#include "src/OpenSimBindings/ParamBlock.hpp"
#include "src/OpenSimBindings/ParamValue.hpp"
#include "src/OpenSimBindings/IntegratorMethod.hpp"
#include "src/Widgets/StandardPopup.hpp"

#include <imgui.h>
#include <nonstd/span.hpp>

#include <string>
#include <utility>
#include <variant>

template<class... Ts>
struct Overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

static bool DrawEditor(osc::ParamBlock& b, int idx, double v)
{
    // note: the input prevision has to be quite high here, because the
    //       ParamBlockEditorPopup has to edit simulation parameters, and
    //       one of those parameters is "Simulation Step Size (seconds)",
    //       which OpenSim defaults to a very very small number (10 ns)
    //
    //       see: #553

    float fv = static_cast<float>(v);
    if (ImGui::InputFloat("##", &fv, 0.0f, 0.0f, "%.9f"))
    {
        b.setValue(idx, static_cast<double>(fv));
        return true;
    }
    else
    {
        return false;
    }
}

static bool DrawEditor(osc::ParamBlock& b, int idx, int v)
{
    if (ImGui::InputInt("##", &v))
    {
        b.setValue(idx, v);
        return true;
    }
    else
    {
        return false;
    }
}

static bool DrawEditor(osc::ParamBlock& b, int idx, osc::IntegratorMethod im)
{
    nonstd::span<char const* const> methodStrings = osc::GetAllIntegratorMethodStrings();
    int method = static_cast<int>(im);

    if (ImGui::Combo("##", &method, methodStrings.data(), static_cast<int>(methodStrings.size())))
    {
        b.setValue(idx, static_cast<osc::IntegratorMethod>(method));
        return true;
    }
    else
    {
        return false;
    }
}

static bool DrawEditor(osc::ParamBlock& b, int idx)
{
    osc::ParamValue v = b.getValue(idx);
    bool rv = false;
    auto handler = Overloaded{
        [&b, &rv, idx](double dv) { rv = DrawEditor(b, idx, dv); },
        [&b, &rv, idx](int iv) { rv = DrawEditor(b, idx, iv); },
        [&b, &rv, idx](osc::IntegratorMethod imv) { rv = DrawEditor(b, idx, imv); },
    };
    std::visit(handler, v);
    return rv;
}

class osc::ParamBlockEditorPopup::Impl final : public StandardPopup {
public:

    Impl(std::string_view popupName, ParamBlock* paramBlock) :
        StandardPopup{std::move(popupName), {512.0f, 0.0f}, ImGuiWindowFlags_AlwaysAutoResize},
        m_OutputTarget{std::move(paramBlock)},
        m_LocalCopy{*m_OutputTarget}
    {
    }

private:
    void implDrawContent() override
    {
        m_WasEdited = false;

        ImGui::Columns(2);
        for (int i = 0, len = m_LocalCopy.size(); i < len; ++i)
        {
            ImGui::PushID(i);

            ImGui::TextUnformatted(m_LocalCopy.getName(i).c_str());
            ImGui::SameLine();
            DrawHelpMarker(m_LocalCopy.getName(i), m_LocalCopy.getDescription(i));
            ImGui::NextColumn();

            if (DrawEditor(m_LocalCopy, i))
            {
                m_WasEdited = true;
            }
            ImGui::NextColumn();

            ImGui::PopID();
        }
        ImGui::Columns();

        ImGui::Dummy({0.0f, 1.0f});

        if (ImGui::Button("save"))
        {
            *m_OutputTarget = m_LocalCopy;
            requestClose();
        }
        ImGui::SameLine();
        if (ImGui::Button("close"))
        {
            requestClose();
        }
    }

    bool m_WasEdited = false;
    ParamBlock* m_OutputTarget = nullptr;
    ParamBlock m_LocalCopy;
};

osc::ParamBlockEditorPopup::ParamBlockEditorPopup(std::string_view popupName, ParamBlock* paramBlock) :
    m_Impl{std::make_unique<Impl>(std::move(popupName), std::move(paramBlock))}
{
}

osc::ParamBlockEditorPopup::ParamBlockEditorPopup(ParamBlockEditorPopup&&) noexcept = default;
osc::ParamBlockEditorPopup& osc::ParamBlockEditorPopup::operator=(ParamBlockEditorPopup&&) noexcept = default;
osc::ParamBlockEditorPopup::~ParamBlockEditorPopup() noexcept = default;

bool osc::ParamBlockEditorPopup::implIsOpen() const
{
    return m_Impl->isOpen();
}

void osc::ParamBlockEditorPopup::implOpen()
{
    m_Impl->open();
}

void osc::ParamBlockEditorPopup::implClose()
{
    m_Impl->close();
}

bool osc::ParamBlockEditorPopup::implBeginPopup()
{
    return m_Impl->beginPopup();
}

void osc::ParamBlockEditorPopup::implDrawPopupContent()
{
    m_Impl->drawPopupContent();
}

void osc::ParamBlockEditorPopup::implEndPopup()
{
    m_Impl->endPopup();
}
