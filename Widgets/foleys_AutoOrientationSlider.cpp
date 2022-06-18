namespace foleys
{
AutoOrientationSlider::AutoOrientationSlider()
{
    setWantsKeyboardFocus (true);
}

void AutoOrientationSlider::setAutoOrientation (bool shouldAutoOrient)
{
    autoOrientation = shouldAutoOrient;
    resized();
}

bool AutoOrientationSlider::keyPressed (const juce::KeyPress& key)
{
    auto getNudgeAmount = [this]()
    {
        if (auto defaultInterval = getInterval(); defaultInterval > 0.0)
            return defaultInterval;

        return getRange().getLength() * 0.01;
    };

    auto announceValueChange = [this]()
    {
        juce::ignoreUnused (this);
        // it seems like this announcement is redundant with most screen readers,
        // but I'll leave it here for now, just in case it needs to be brought back later.
        // const auto announcement = "Slider: " + getName() + ", new value: " + getTextFromValue (getValue());
        // juce::AccessibilityHandler::postAnnouncement (announcement, juce::AccessibilityHandler::AnnouncementPriority::low);
    };

    if (key == juce::KeyPress::upKey)
    {
        setValue (juce::jmin (getValue() + getNudgeAmount(), getRange().getEnd()), juce::sendNotification);
        announceValueChange();
        return true;
    }

    if (key == juce::KeyPress::downKey)
    {
        setValue (juce::jmax (getValue() - getNudgeAmount(), getRange().getStart()), juce::sendNotification);
        announceValueChange();
        return true;
    }

    if (key == juce::KeyPress::pageUpKey)
    {
        setValue (juce::jmin (getValue() + 10.0 * getNudgeAmount(), getRange().getEnd()), juce::sendNotification);
        announceValueChange();
        return true;
    }

    if (key == juce::KeyPress::pageDownKey)
    {
        setValue (juce::jmax (getValue() - 10.0 * getNudgeAmount(), getRange().getStart()), juce::sendNotification);
        announceValueChange();
        return true;
    }

    if (key == juce::KeyPress::deleteKey)
    {
        setValue (getDoubleClickReturnValue(), juce::sendNotification);
        announceValueChange();
        return true;
    }

    if (key == juce::KeyPress::homeKey)
    {
        setValue (getRange().getStart(), juce::sendNotification);
        announceValueChange();
        return true;
    }

    if (key == juce::KeyPress::endKey)
    {
        setValue (getRange().getEnd(), juce::sendNotification);
        announceValueChange();
        return true;
    }

    if (key == juce::KeyPress::returnKey && getTextBoxPosition() != NoTextBox)
    {
        showTextBox();
        return true;
    }

    return false;
}

void AutoOrientationSlider::resized()
{
    if (autoOrientation)
    {
        const auto w = getWidth();
        const auto h = getHeight();

        if (w > 2 * h)
            setSliderStyle (juce::Slider::LinearHorizontal);
        else if (h > 2 * w)
            setSliderStyle (juce::Slider::LinearVertical);
        else
            setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    }

    juce::Slider::resized();
}

void AutoOrientationSlider::mouseDrag (const juce::MouseEvent& e)
{
    auto numSources = juce::Desktop::getInstance().getNumDraggingMouseSources();
    if (numSources > 1)
        return;

    juce::Slider::mouseDrag (e);
}
}
