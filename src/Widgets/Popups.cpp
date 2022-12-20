#include "Popups.hpp"

#include "src/Utils/Algorithms.hpp"
#include "src/Widgets/Popup.hpp"

#include <cstddef>

osc::Popups::Popups() = default;
osc::Popups::Popups(Popups&&) noexcept = default;
osc::Popups& osc::Popups::operator=(Popups&&) noexcept = default;
osc::Popups::~Popups() noexcept = default;

void osc::Popups::push_back(std::shared_ptr<Popup> popup)
{
    m_Popups.push_back(std::move(popup));
}

void osc::Popups::draw()
{
    // begin and (if applicable) draw bottom-to-top in a nested fashion
    ptrdiff_t nOpened = 0;
    size_t nPopups = m_Popups.size();  // only draw the popups that existed at the start of this frame, not the ones added during this frame
    for (size_t i = 0; i < nPopups; ++i)
    {
        if (m_Popups[i]->beginPopup())
        {
            m_Popups[i]->drawPopupContent();
            ++nOpened;
        }
        else
        {
            break;
        }
    }

    // end the opened popups top-to-bottom
    for (ptrdiff_t i = nOpened-1; i >= 0; --i)
    {
        m_Popups[i]->endPopup();
    }

    // garbage-collect any closed popups
    osc::RemoveErase(m_Popups, [](auto const& ptr) { return !ptr->isOpen(); });
}