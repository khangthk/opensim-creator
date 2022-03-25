#pragma once

#include "src/OpenSimBindings/VirtualOutput.hpp"
#include "src/Utils/ClonePtr.hpp"
#include "src/Utils/UID.hpp"

#include <nonstd/span.hpp>

#include <optional>
#include <string_view>
#include <string>
#include <vector>


namespace OpenSim
{
    class AbstractOutput;
    class Component;
    class ComponentPath;
    class Model;
}

namespace osc
{
    class SimulationReport;
}

namespace SimTK
{
    class State;
}


namespace osc
{
    enum class OutputSubfield {
        None = 0,
        X = 1<<0,
        Y = 1<<1,
        Z = 1<<2,
        Magnitude = 1<<3,
        Default = None,
    };

    char const* GetOutputSubfieldLabel(OutputSubfield);
    nonstd::span<OutputSubfield const> GetAllSupportedOutputSubfields();

    // returns applicable OutputSubfield ORed together
    int GetSupportedSubfields(OpenSim::AbstractOutput const&);

    // a virtual output that extracts values from an OpenSim::AbstractOutput
    class ComponentOutput final : public VirtualOutput {
    public:
        ComponentOutput(OpenSim::AbstractOutput const&,
                        OutputSubfield = OutputSubfield::None);
        ComponentOutput(ComponentOutput const&);
        ComponentOutput(ComponentOutput&&) noexcept;
        ComponentOutput& operator=(ComponentOutput const&);
        ComponentOutput& operator=(ComponentOutput&&) noexcept;
        ~ComponentOutput() noexcept;

        std::string const& getName() const override;
        std::string const& getDescription() const override;

        OutputType getOutputType() const override;
        float getValueFloat(OpenSim::Component const&, SimulationReport const&) const override;
        void getValuesFloat(OpenSim::Component const&, nonstd::span<SimulationReport const>, nonstd::span<float> overwriteOut) const override;
        std::string getValueString(OpenSim::Component const&, SimulationReport const&) const override;

        class Impl;
    private:
        ClonePtr<Impl> m_Impl;
    };
}
