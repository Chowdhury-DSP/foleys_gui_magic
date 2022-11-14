/*
 ==============================================================================
    Copyright (c) 2019-2020 Foleys Finest Audio Ltd. - Daniel Walz
    All rights reserved.

    License for non-commercial projects:

    Redistribution and use in source and binary forms, with or without modification,
    are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    License for commercial products:

    To sell commercial products containing this module, you are required to buy a
    License from https://foleysfinest.com/developer/pluginguimagic/

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
    OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
    OF THE POSSIBILITY OF SUCH DAMAGE.
 ==============================================================================
 */

namespace foleys
{

Container::Container (MagicGUIBuilder& builder, juce::ValueTree node)
  : GuiItem (builder, node)
{
    setFocusContainerType (FocusContainerType::keyboardFocusContainer);
}

void Container::update()
{
    configureFlexBox (configNode);

    for (auto& child : *this)
        child->updateInternal();

    const auto display = magicBuilder.getStyleProperty (IDs::display, configNode).toString();
    if (display == IDs::contents)
        setLayoutMode (Container::Layout::Contents);
    else if (display == IDs::tabbed)
        setLayoutMode (Container::Layout::Tabbed);
    else
        setLayoutMode (Container::Layout::FlexBox);

    auto repaintHz = magicBuilder.getStyleProperty (IDs::repaintHz, configNode).toString();
    if (repaintHz.isNotEmpty())
    {
        refreshRateHz = repaintHz.getIntValue();
        updateContinuousRedraw();
    }
}

void Container::addChildItem (std::unique_ptr<GuiItem> child)
{
    addAndMakeVisible (child.get());
    children.push_back (std::move (child));
}

void Container::createSubComponents()
{
    children.clear();

    for (auto childNode : configNode)
    {
        auto childItem = magicBuilder.createGuiItem (childNode);
        if (childItem)
        {
            const auto group = magicBuilder.getStyleProperty (IDs::group, childNode).toString();
            if (group.isEmpty())
            {
                addAndMakeVisible (childItem.get());
            }
            else
            {
                addComponentToAccessibilityGroup (group, childItem.get());
            }

            childItem->createSubComponents();

            children.push_back (std::move (childItem));
        }
    }

    updateLayout();
    updateContinuousRedraw();
}

void Container::addComponentToAccessibilityGroup (const juce::String& groupName, juce::Component* comp)
{
    {
        int index = 0;
        for (const auto& name : accessibilityGroupNames)
        {
            if (name == groupName)
            {
                accessibilityGroups[index]->addAndMakeVisible (comp);
                return;
            }

            index++;
        }
    }

    struct AccessibilityTabGroup : juce::Component
    {
        AccessibilityTabGroup (Container& cont, int containerIndex) : container (cont),
                                                                      index (containerIndex)
        {
            setWantsKeyboardFocus (true);
            setFocusContainerType (FocusContainerType::focusContainer);
            setAccessible (true);

            setComponentID ("accessibility_group");
        }

        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
        {
            if (container.layout == Layout::Tabbed)
            {
                return std::make_unique<juce::AccessibilityHandler> (
                    *this,
                    juce::AccessibilityRole::group,
                    juce::AccessibilityActions()
                        .addAction (juce::AccessibilityActionType::press, [this] { selectThisTab (false); }));
            }

            return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::group);
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::returnKey)
            {
                if (container.layout == Layout::Tabbed && container.tabbedButtons->getCurrentTabIndex() != index)
                {
                    selectThisTab (false);
                }
                else
                {
                    if (auto* currentlyFocusedGroup = dynamic_cast<AccessibilityTabGroup*> (getCurrentlyFocusedComponent()))
                        currentlyFocusedGroup->getChildren().getFirst()->grabKeyboardFocus();
                }

                return true;
            }

            return false;
        }

        void selectThisTab (bool announce)
        {
            container.tabbedButtons->setCurrentTabIndex (index);

            if (announce)
            {
                const auto announcement = "Selected: " + getTitle().upToFirstOccurrenceOf ("(", false, false);
                static constexpr auto priority = juce::AccessibilityHandler::AnnouncementPriority::medium;
                juce::AccessibilityHandler::postAnnouncement (announcement, priority);
            }
        }

        Container& container;
        const int index;
    };

    accessibilityGroupNames.push_back (groupName);
    auto* newGroup = accessibilityGroups.add (std::make_unique<AccessibilityTabGroup> (*this,
                                                                                       (int) accessibilityGroups.size()));

    newGroup->setTitle (groupName);
    newGroup->addAndMakeVisible (comp);
    newGroup->setInterceptsMouseClicks (false, true);
    addAndMakeVisible (newGroup);
}

GuiItem* Container::findGuiItemWithId (const juce::String& name)
{
    if (configNode.getProperty (IDs::id, juce::String()).toString() == name)
        return this;

    for (auto& item : children)
        if (auto* matching = item->findGuiItemWithId (name))
            return matching;

    return nullptr;
}

void Container::setLayoutMode (Layout layoutToUse)
{
    layout = layoutToUse;
    if (layout == Layout::Tabbed)
    {
        updateTabbedButtons();
    }
    else
    {
        tabbedButtons.reset();
        for (auto& child : children)
            child->setVisible (true);
    }

    updateLayout();
}

void Container::resized()
{
    for (auto* group : accessibilityGroups)
        group->setBounds (getLocalBounds());

    updateLayout();
}

void Container::updateLayout()
{
    if (children.empty())
        return;

    auto clientBounds = getClientBounds();

    if (layout == Layout::FlexBox)
    {
        flexBox.items.clear();
        for (auto& child : children)
            flexBox.items.add (child->getFlexItem());

        flexBox.performLayout (clientBounds);
    }
    else
    {
        if (layout == Layout::Tabbed)
        {
            updateTabbedButtons();
            tabbedButtons->setBounds (clientBounds.removeFromTop (40));
        }
        else
            tabbedButtons.reset();

        for (auto& child : children)
            child->setBounds (clientBounds);
    }

    for (auto& child : children)
        child->updateLayout();
}

void Container::updateColours()
{
    decorator.updateColours (magicBuilder, configNode);

    for (auto& child : children)
        child->updateColours();
}

void Container::updateContinuousRedraw()
{
    stopTimer();
    plotComponents.clear();

    for (auto& child : children)
        if (auto* p = dynamic_cast<MagicPlotComponent*>(child->getWrappedComponent()))
            plotComponents.push_back (p);

    if (! plotComponents.empty())
        startTimerHz (refreshRateHz);
}

void Container::updateTabbedButtons()
{
    tabbedButtons = std::make_unique<juce::TabbedButtonBar>(juce::TabbedButtonBar::TabsAtTop);
    addAndMakeVisible (*tabbedButtons);

    for (auto& child : children)
    {
        tabbedButtons->addTab (child->getTabCaption ("Tab " + juce::String (tabbedButtons->getNumTabs())),
                               child->getTabColour(), -1);

        auto* newButton = tabbedButtons->getTabButton (tabbedButtons->getNumTabs() - 1);
        newButton->setAccessible (false);
        newButton->setWantsKeyboardFocus (true);
        newButton->setDescription ("Tab: " + newButton->getName());
    }

    tabbedButtons->addChangeListener (this);
    tabbedButtons->setCurrentTabIndex (currentTab, false);
    updateSelectedTab();
}

void Container::configureFlexBox (const juce::ValueTree& node)
{
    flexBox = juce::FlexBox();

    const auto direction = magicBuilder.getStyleProperty (IDs::flexDirection, node).toString();
    if (direction == IDs::flexDirRow)
        flexBox.flexDirection = juce::FlexBox::Direction::row;
    else if (direction == IDs::flexDirRowReverse)
        flexBox.flexDirection = juce::FlexBox::Direction::rowReverse;
    else if (direction == IDs::flexDirColumn)
        flexBox.flexDirection = juce::FlexBox::Direction::column;
    else if (direction == IDs::flexDirColumnReverse)
        flexBox.flexDirection = juce::FlexBox::Direction::columnReverse;

    const auto wrap = magicBuilder.getStyleProperty (IDs::flexWrap, node).toString();
    if (wrap == IDs::flexWrapNormal)
        flexBox.flexWrap = juce::FlexBox::Wrap::wrap;
    else if (wrap == IDs::flexWrapReverse)
        flexBox.flexWrap = juce::FlexBox::Wrap::wrapReverse;
    else
        flexBox.flexWrap = juce::FlexBox::Wrap::noWrap;

    const auto alignContent = magicBuilder.getStyleProperty (IDs::flexAlignContent, node).toString();
    if (alignContent == IDs::flexStart)
        flexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
    else if (alignContent == IDs::flexEnd)
        flexBox.alignContent = juce::FlexBox::AlignContent::flexEnd;
    else if (alignContent == IDs::flexCenter)
        flexBox.alignContent = juce::FlexBox::AlignContent::center;
    else if (alignContent == IDs::flexSpaceAround)
        flexBox.alignContent = juce::FlexBox::AlignContent::spaceAround;
    else if (alignContent == IDs::flexSpaceBetween)
        flexBox.alignContent = juce::FlexBox::AlignContent::spaceBetween;
    else
        flexBox.alignContent = juce::FlexBox::AlignContent::stretch;

    const auto alignItems = magicBuilder.getStyleProperty (IDs::flexAlignItems, node).toString();
    if (alignItems == IDs::flexStart)
        flexBox.alignItems = juce::FlexBox::AlignItems::flexStart;
    else if (alignItems == IDs::flexEnd)
        flexBox.alignItems = juce::FlexBox::AlignItems::flexEnd;
    else if (alignItems == IDs::flexCenter)
        flexBox.alignItems = juce::FlexBox::AlignItems::center;
    else
        flexBox.alignItems = juce::FlexBox::AlignItems::stretch;

    const auto justify = magicBuilder.getStyleProperty (IDs::flexJustifyContent, node).toString();
    if (justify == IDs::flexEnd)
        flexBox.justifyContent = juce::FlexBox::JustifyContent::flexEnd;
    else if (justify == IDs::flexCenter)
        flexBox.justifyContent = juce::FlexBox::JustifyContent::center;
    else if (justify == IDs::flexSpaceAround)
        flexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    else if (justify == IDs::flexSpaceBetween)
        flexBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    else
        flexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;
}

void Container::timerCallback()
{
    for (auto p : plotComponents)
    {
        if(p->needsUpdate())
            p->repaint();
    }
}

void Container::changeListenerCallback (juce::ChangeBroadcaster*)
{
    currentTab = tabbedButtons ? tabbedButtons->getCurrentTabIndex() : 0;
    updateSelectedTab();
}

void Container::updateSelectedTab()
{
    {
        juce::ScopedValueSetter<bool> svs { skipSettingTabAsDefaultComponent, true };

        int index = 0;
        for (auto& child : children)
        {
            const auto isSelected = (currentTab == index);
            if (tabbedButtons != nullptr)
            {
                if (auto* tabButton = tabbedButtons->getTabButton (index))
                    tabButton->setDescription ("Tab: " + tabButton->getName() + (isSelected ? " (Selected)" : ""));
            }

            child->setVisible (isSelected);

            if (isSelected && isShowing())
            {
                if (auto* childContainer = dynamic_cast<Container*> (child.get()))
                    childContainer->shouldFocusFirstComponent = shouldFocusFirstComponent;

                child->grabKeyboardFocus();
                shouldFocusFirstComponent = true; // reset this to the default setting every time
            }

            index++;
        }
    }

    {
        int index = 0;
        for (auto* group : accessibilityGroups)
        {
            const auto isSelected = (currentTab == index);
            const auto groupTitle = accessibilityGroupNames[(size_t) index] + (isSelected ? " (Selected}" : " (Not Selected)");
            group->setTitle (groupTitle);
            index++;
        }
    }
}

std::vector<std::unique_ptr<GuiItem>>::iterator Container::begin()
{
    return children.begin();
}

std::vector<std::unique_ptr<GuiItem>>::iterator Container::end()
{
    return children.end();
}

std::unique_ptr<juce::AccessibilityHandler> Container::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::group);
}

std::unique_ptr<juce::ComponentTraverser> Container::createKeyboardFocusTraverser()
{
    struct ContainerKeyboardFocusTraverser : juce::KeyboardFocusTraverser
    {
        ContainerKeyboardFocusTraverser (Container& cont) : container (cont) {}

        Component* traverseSkippingSliderSubComponents (Component* current, bool forwards, std::function<Component* (Component*)>&& traverseFunc)
        {
            if (auto* next = traverseFunc (current))
            {
                if (dynamic_cast<juce::Slider*> (next->getParentComponent()) != nullptr)
                    return traverseSkippingSliderSubComponents (next, forwards, std::move (traverseFunc));

                if (next->getComponentID() == "accessibility_group")
                    return traverseSkippingSliderSubComponents (next, forwards, std::move (traverseFunc));

                return next;
            }

            auto* parent = current->getParentComponent();
            while (parent != nullptr)
            {
                if (parent == &container)
                {
                    parent = container.getParentComponent();
                    foleys::Container* currentGroup = &container;
                    while (parent != nullptr)
                    {
                        if (auto* magicEditor = dynamic_cast<MagicPluginEditor*> (parent))
                        {
                            Container* rootContainer = nullptr;
                            for (auto* comp : magicEditor->getChildren())
                            {
                                rootContainer = dynamic_cast<Container*> (comp);
                                if (rootContainer != nullptr)
                                    break;
                            }

                            jassert (rootContainer != nullptr); // editor has no root node?!?!?!

                            // we're at the top-most level now...
                            auto& nextComp = forwards ? rootContainer->children.front() : rootContainer->children.back();
                            if (auto* nextContainer = dynamic_cast<Container*> (nextComp.get()))
                                nextContainer->shouldFocusFirstComponent = forwards;

                            return nextComp.get();
                        }

                        if (auto* parentContainer = dynamic_cast<Container*> (parent))
                        {
                            if (parentContainer->layout == Layout::Tabbed)
                            {
                                currentGroup = parentContainer;
                                const auto newTabIndex = parentContainer->tabbedButtons->getCurrentTabIndex() + (forwards ? 1 : -1);
                                if (juce::isPositiveAndBelow (newTabIndex, parentContainer->tabbedButtons->getNumTabs()))
                                {
                                    parentContainer->shouldFocusFirstComponent = forwards;
                                    parentContainer->tabbedButtons->setCurrentTabIndex (newTabIndex);
                                    return nullptr;
                                }
                            }
                            else
                            {
                                auto& currentLevelChildren = parentContainer->children;
                                if ((currentGroup == currentLevelChildren.back().get() && forwards) || (currentGroup == currentLevelChildren.front().get() && ! forwards))
                                {
                                    // end of this group! we need to go up a level...
                                    currentGroup = parentContainer;
                                }
                                else
                                {
                                    auto&& currentGroupIter = std::find_if (currentLevelChildren.begin(), currentLevelChildren.end(), [currentGroup] (auto& child)
                                                                                 { return child.get() == currentGroup; });
                                    jassert (currentGroupIter != currentLevelChildren.end()); // why is the current group not a child of the current parent??

                                    while (*currentGroupIter != nullptr)
                                    {
                                        auto* newGroup = forwards ? (++currentGroupIter)->get() : (--currentGroupIter)->get();
                                        if (auto* newGroupContainer = dynamic_cast<foleys::Container*> (newGroup))
                                        {
                                            newGroupContainer->shouldFocusFirstComponent = forwards;
                                            return newGroupContainer;
                                        }

                                        if (! forwards && currentGroupIter == currentLevelChildren.begin())
                                            break;

                                        if (forwards && *currentGroupIter == currentLevelChildren.back())
                                            break;
                                    }

                                    // There's no more containers n this group! we need to go up a level...
                                    currentGroup = parentContainer;
                                }
                            }
                        }

                        parent = parent->getParentComponent();
                    }

                    break;
                }

                parent = parent->getParentComponent();
            }

            return nullptr;
        }

        Component* getNextComponent (Component* current) override
        {
            return traverseSkippingSliderSubComponents (current, true, [this] (Component* cur)  { return juce::KeyboardFocusTraverser::getNextComponent (cur); });
        }

        Component* getPreviousComponent (Component* current) override
        {
            return traverseSkippingSliderSubComponents (current, false, [this] (Component* cur)  { return juce::KeyboardFocusTraverser::getPreviousComponent (cur); });
        }

        std::vector<Component*> getAllComponents (Component* parentComponent) override
        {
            auto&& components = juce::KeyboardFocusTraverser::getAllComponents (parentComponent);
            for (auto compIter = components.begin(); compIter != components.end(); )
            {
                if (dynamic_cast<juce::Slider*> ((*compIter)->getParentComponent()) != nullptr)
                {
                    compIter = components.erase (compIter);
                }
                else if ((*compIter)->getComponentID() == "accessibility_group")
                {
                    compIter = components.erase (compIter);
                }
                else
                {
                    ++compIter;
                }
            }

            return std::move (components);
        }

        Component* getDefaultComponent (Component* parentComponent) override
        {
            Component* returnComp = nullptr;

            const auto& defaultComponents = getAllComponents (parentComponent);
            auto isKeyboardFocusable = [] (const Component* comp, const Component* cont)
            {
                return comp->getWantsKeyboardFocus() && cont->isParentOf (comp);
            };

            if (container.shouldFocusFirstComponent)
            {
                for (auto* comp : defaultComponents)
                {
                    if (isKeyboardFocusable (comp, parentComponent))
                    {
                        returnComp = comp;
                        break;
                    }
                }
            }
            else
            {
                for (auto it = defaultComponents.rbegin(); it != defaultComponents.rend(); ++it)
                {
                    auto* comp = *it;
                    if (isKeyboardFocusable (comp, parentComponent))
                    {
                        returnComp = comp;
                        break;
                    }
                }
            }

            const auto& components = container.children;
            if (returnComp == nullptr && ! components.empty())
            {
                returnComp = container.shouldFocusFirstComponent ? components.front().get() : components.back().get();

                if (auto* returnCompContainer = dynamic_cast<foleys::Container*> (returnComp))
                {
                    returnCompContainer->shouldFocusFirstComponent = container.shouldFocusFirstComponent;
                    if (! returnCompContainer->skipSettingTabAsDefaultComponent)
                    {
                        if (container.layout == Layout::Tabbed)
                        {
                            container.tabbedButtons->setCurrentTabIndex (container.shouldFocusFirstComponent ? 0 : container.tabbedButtons->getNumTabs() - 1);
                            return returnComp;
                        }
                    }
                }
            }

            container.shouldFocusFirstComponent = true; // reset this to the default setting every time

            if (returnComp == parentComponent)
                returnComp = nullptr; // no infinite recursion!

            return returnComp;
        }

        Container& container;
    };

    return std::make_unique<ContainerKeyboardFocusTraverser> (*this);
//    return std::make_unique<juce::KeyboardFocusTraverser>();
}

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
void Container::setEditMode (bool shouldEdit)
{
    for (auto& child : children)
        child->setEditMode (shouldEdit);

    GuiItem::setEditMode (shouldEdit);
}
#endif

} // namespace foleys
