// A Simple RPN calculator application
#define IMGUI_DEFINE_MATH_OPERATORS
#include "hello_imgui/hello_imgui.h"
#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "imgui_internal.h"
#include "rpn_calculator.h"

#include "nlohmann_json.hpp"
#include <map>
#include <optional>
#include <tuple>

#include "rpn_calculator.h"

using namespace RpnCalculator;


std::map<ButtonType, ImVec4> ButtonColors = {
    { ButtonType::Digit, { 0.5f, 0.5f, 0.5f, 1.0f } },
    { ButtonType::DirectNumber, { 0.6f, 0.6f, 0.6f, 1.0f } },
    { ButtonType::Backspace, { 0.65f, 0.65f, 0.65f, 1.0f } },

    { ButtonType::BinaryOperator, { 0.2f, 0.2f, 0.8f, 1.0f } },
    { ButtonType::UnaryOperator, { 0.4f, 0.4f, 0.8f, 1.0f } },
    { ButtonType::StackOperator, { 0.4f, 0.3f, 0.3f, 1.0f } },
    { ButtonType::Inv, { 0.8f, 0.6f, 0.0f, 1.0f } },
    { ButtonType::DegRadGrad, { 0.6f, 0.6f, 0.0f, 1.0f } },
    { ButtonType::Enter, { 0.0f, 0.7f, 0.0f, 1.0f } },
};


struct AppState
{
    ImFont* ButtonFont = nullptr, *LDCFont = nullptr, *SmallFont = nullptr;
    CalculatorState CalculatorState;
};


bool ButtonWithGradient(const char*label, ImVec2 buttonSize, ImVec4 color)
{
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    bool pressed = ImGui::Button(label, buttonSize);

    // Draw a gradient on top of the button
    {
        ImVec2 tl = ImGui::GetItemRectMin();
        ImVec2 br = ImGui::GetItemRectMax();
        ImVec2 size = ImGui::GetItemRectSize();

        float k = 0.3f;

        ImVec2 tl_middle(tl.x, tl.y + size.y * (1.f - k));
        ImVec2 br_middle(br.x, tl.y + size.y * k);

        ImVec4 col_darker(0.f, 0.f, 0.f, 0.2f);
        ImVec4 col_interm(0.f, 0.f, 0.f, 0.1f);
        ImVec4 col_transp(0.f, 0.f, 0.f, 0.f);

        ImGui::GetForegroundDrawList()->AddRectFilledMultiColor(
            tl,
            br_middle,
            ImGui::GetColorU32(col_interm), // upper left
            ImGui::GetColorU32(col_interm), // upper right
            ImGui::GetColorU32(col_transp), // bottom right
            ImGui::GetColorU32(col_transp)  // bottom left
        );
        ImGui::GetForegroundDrawList()->AddRectFilledMultiColor(
            tl_middle,
            br,
            ImGui::GetColorU32(col_transp), // upper left
            ImGui::GetColorU32(col_transp), // upper right
            ImGui::GetColorU32(col_darker), // bottom right
            ImGui::GetColorU32(col_darker)  // bottom left
        );

    }
    ImGui::PopStyleColor();
    return pressed;
}


void DrawIconOnLastButton(const std::string& iconPath, ImVec2 iconSize)
{
    ImVec2 tl = ImGui::GetItemRectMin();
    ImVec2 br = ImGui::GetItemRectMax();
    ImVec2 center = (tl + br) * 0.5f;
    auto iconTexture = HelloImGui::ImTextureIdFromAsset(iconPath.c_str());
    ImGui::GetForegroundDrawList()->AddImage(
        iconTexture,
        center - iconSize * 0.5f,
        center + iconSize * 0.5f,
        ImVec2(0.f, 0.f),
        ImVec2(1.f, 1.f),
        ImGui::GetColorU32(ImGuiCol_Text)
    );
}


void DrawCalculatorKeyLabelOnLastButton(const std::string& label, ImFont* smallFont)
{
    // Draw the button label (with possible exponent)

    // if the label contains "^" we will need to split in two parts
    bool isExponent = (label.find('^') != std::string::npos);
    std::string labelStd, labelExponent;
    if (!isExponent)
        labelStd = label;
    else
    {
        labelStd = label.substr(0, label.find('^'));
        labelExponent = label.substr(label.find('^') + 1);
    }
    {
        ImVec2 buttonPosition = ImGui::GetItemRectMin();
        ImVec2 buttonSize = ImGui::GetItemRectSize();

        ImVec2 sizeStd = ImGui::CalcTextSize(labelStd.c_str());
        ImGui::PushFont(smallFont);
        ImVec2 sizeExponent = ImGui::CalcTextSize(labelExponent.c_str());
        ImGui::PopFont();

        ImVec2 labelStdPosition(
            buttonPosition.x + (buttonSize.x - sizeStd.x - sizeExponent.x) * 0.5f,
            buttonPosition.y + (buttonSize.y - sizeStd.y) * 0.5f);
        ImGui::GetForegroundDrawList()->AddText(
            labelStdPosition,
            ImGui::GetColorU32(ImGuiCol_Text),
            labelStd.c_str());

        ImVec2 labelExponentPosition(
            buttonPosition.x + (buttonSize.x - sizeStd.x - sizeExponent.x) * 0.5f + sizeStd.x,
            buttonPosition.y + (buttonSize.y - sizeStd.y) * 0.5f);
        ImGui::PushFont(smallFont);
        ImGui::GetForegroundDrawList()->AddText(
            labelExponentPosition,
            ImGui::GetColorU32(ImGuiCol_Text),
            labelExponent.c_str());
        ImGui::PopFont();
    }
}


// Draw a button with a size that fits 4 buttons per row and a color based on the button type
bool DrawOneCalculatorButton(
    const CalculatorButton& button,
    bool inverseMode,
    ImVec2 standardSize,
    ImVec2 doubleButtonSize,
    ImFont* smallFont
    )
{
    ImVec2 buttonSize =  button.IsDoubleWidth ? doubleButtonSize : standardSize;
    ImVec4 color = ButtonColors[button.Type];
    if (inverseMode)
        color = ImVec4(0.6f, 0.4f, 0.4f, 1.f);

    // Draw the button with an invisible label
    std::string id = std::string("##") + button.Label;
    bool pressed = ButtonWithGradient(id.c_str(), buttonSize, color);

    // Draw icon if available, else draw the key label (with possible exponent)
    static std::map<std::string, std::string> btnSpecificIcons{
        { "Pi", "images/pi100white.png"},
        { "sqrt", "images/sqrt100white.png"},
        { "<=", "images/backspace100white.png"}
    };
    if (btnSpecificIcons.find(button.Label) != btnSpecificIcons.end())
    {
        ImVec2 iconSize = HelloImGui::EmToVec2(0.7f, 0.7f);
        if (button.Label == "<=")
            iconSize = iconSize * 1.5f;
        DrawIconOnLastButton(btnSpecificIcons[button.Label], iconSize);
    }
    else
        DrawCalculatorKeyLabelOnLastButton(button.Label, smallFont);

    return pressed;
}

auto ComputeButtonsSizes(int nbRows, int nbCols, float calculatorBorderMargin)
{
    ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
    ImVec2 totalButtonsSpacing(
        spacing.x * ((float)nbCols - 1.f),
        spacing.y * ((float)nbRows - 1.f)
    );

    float buttonWidth = (ImGui::GetWindowWidth() - totalButtonsSpacing.x - calculatorBorderMargin * 2.f) / 4.f;

    float remainingHeight =
        ImGui::GetWindowHeight()
        - ImGui::GetCursorPosY()
        - totalButtonsSpacing.y
        - calculatorBorderMargin * 2.f;
    float buttonHeight = remainingHeight / (float)nbRows;

    ImVec2 standardButtonSize = ImVec2(buttonWidth, buttonHeight);
    ImVec2 doubleButtonSize = ImVec2(standardButtonSize.x * 2 + ImGui::GetStyle().ItemSpacing.x, standardButtonSize.y);
    return std::make_tuple(standardButtonSize, doubleButtonSize);
}

// Layout 4 buttons per row (with possible double width)
// Returns the pressed button
std::optional<CalculatorButton> LayoutButtons(AppState& appState)
{
    ImGui::PushFont(appState.ButtonFont);
    float calculatorBorderMargin = HelloImGui::EmSize(0.5f);
    ImGui::GetStyle().ItemSpacing = {calculatorBorderMargin, calculatorBorderMargin};

    const auto& buttonsRows = appState.CalculatorState.CalculatorLayoutDefinition.Buttons;
    int nbRows = (int)buttonsRows.size();
    int nbCols = appState.CalculatorState.CalculatorLayoutDefinition.NbButtonsPerRow;

    // Calculate buttons size so that they fit the remaining space
    auto [standardButtonSize, doubleButtonSize] = ComputeButtonsSizes(nbRows, nbCols, calculatorBorderMargin);

    // Display the buttons, and return the pressed button (if any)
    std::optional<CalculatorButton> pressedButton;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + calculatorBorderMargin);
    for (const auto& buttonRow: buttonsRows)
    {
        ImGui::SetCursorPosX(calculatorBorderMargin);
        for (const auto& buttonWithInverse: buttonRow)
        {
            bool inverseMode = appState.CalculatorState.InverseMode && buttonWithInverse.InverseButton.has_value();
            const CalculatorButton& button = buttonWithInverse.GetCurrentButton(inverseMode);
            if (DrawOneCalculatorButton(button, inverseMode, standardButtonSize, doubleButtonSize, appState.SmallFont))
                pressedButton = button;
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }
    ImGui::PopFont();
    return pressedButton;
}


void GuiDisplay(AppState& appState)
{
    const auto& calculatorState = appState.CalculatorState;

    //
    // Display the RPN number stack in a child window that resembles a LCD display
    //
    // Create a child window with a grey background color
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.75f, 0.75f, 0.75f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 1.f));
    ImGui::SetCursorPos(ImGui::GetStyle().ItemSpacing);
    float childWith = ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x * 2.f;
    ImGui::BeginChild("StackDisplay", ImVec2(childWith, 0), ImGuiChildFlags_AutoResizeY);

    // Display small indicator messages at top of the display (angle unit, inverse mode)
    {
        ImGui::PushFont(appState.SmallFont);
        // Display angle unit
        std::string angleUnitStr = to_string(calculatorState.AngleUnit);
        ImGui::SameLine((float)(int)(calculatorState.AngleUnit) * HelloImGui::EmSize(2.f));
        ImGui::Text("%s", angleUnitStr.c_str());

        // Display Inv indicator on the same line,but at the right
        if (calculatorState.InverseMode)
        {
            ImGui::SameLine(ImGui::GetWindowWidth() - HelloImGui::EmSize(2.f));
            ImGui::Text("Inv");
        }
        ImGui::PopFont();
    }

    // Display the stack
    ImGui::PushFont(appState.LDCFont);
    int nbViewableStackLines = calculatorState.CalculatorLayoutDefinition.DisplayedStackSize;
    for (int i = 0; i < nbViewableStackLines; ++i)
    {
        int stackIndex = (int)calculatorState.Stack.size() - nbViewableStackLines + i;
        if (stackIndex < 0)
            ImGui::Text("");
        else
        {
            ImGui::Text("%i:", nbViewableStackLines - i);
            // Display the stack value at the right of the screen
            // Convert value to string with a fixed number of decimals
            int nbDecimals = calculatorState.CalculatorLayoutDefinition.NbDecimals;
            char valueAsString[64];
            snprintf(valueAsString, 64, "%.*G", nbDecimals, calculatorState.Stack[stackIndex]);
            ImVec2 textSize = ImGui::CalcTextSize(valueAsString);
            ImGui::SameLine(ImGui::GetWindowWidth() - textSize.x);
            ImGui::Text("%s", valueAsString);
        }
    }

    // Display the input number being entered by the user
    ImGui::Text("%s", calculatorState.Input.c_str());
    ImGui::PopFont();

    // Display error message (if any)
    ImGui::PushFont(appState.SmallFont);
    ImGui::Text("%s", calculatorState.ErrorMessage.c_str());
    ImGui::PopFont();

    ImGui::PopStyleColor(2);
    ImGui::EndChild();
}



void HandleComputerKeyboardOld(CalculatorState& calculatorState)
{
    // Draw an invisible input text field to capture keyboard input
    static char buffer[2000] = "";

    // We need to change the InputTextMultiline's id at each user input, in order to be able to reset it
    // (it seems like ImGui caches values somewhere)
    static int idx = 0;
    std::string id = std::string("##hidden_txt") + std::to_string(idx);

    // Make sure the next input text has focus, so it can capture keyboard input
    // (but do not steal it if the user is mousing)
    if (!ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemActive())
        ImGui::SetKeyboardFocusHere();
    ImVec2 textSize(1.f, 1.f);
    if (ImGui::InputTextMultiline(id.c_str(), buffer, 256, textSize))
    {
        if (strlen(buffer) == 1)
            calculatorState.OnComputerKey(buffer[0]);
        // Change the id for the next frame
        ++idx;
        // So that ImGui accepts to take into account that the buffer was reset
        buffer[0] = '\0';
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
        calculatorState.OnComputerKey('\b');
}

static int TextEditCallbackStub(ImGuiInputTextCallbackData* data) {
    // You can store your state object in the `UserData` field of the callback data structure
    CalculatorState* calculatorState = static_cast<CalculatorState*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter)
    {
        // This event is triggered for every character input.
        char c = static_cast<char>(data->EventChar);
        calculatorState->OnComputerKey(c);
    }
    return 0;
}

void HandleComputerKeyboard(CalculatorState& calculatorState)
{
    // Draw an invisible input text field to capture keyboard input
    static char buffer[2000] = "";

    // Make sure the next input text has focus, so it can capture keyboard input
    // (but do not steal it if the user is mousing)
    if (!ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemActive())
        ImGui::SetKeyboardFocusHere();
    ImVec2 textSize(1.f, 1.f);
    ImGui::InputTextMultiline("##hidden_input", buffer, IM_ARRAYSIZE(buffer), textSize,
                     ImGuiInputTextFlags_CallbackCharFilter, TextEditCallbackStub, &calculatorState);

    if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
        calculatorState.OnComputerKey('\b');

}

void ShowGui(AppState& appState)
{
    GuiDisplay(appState);
    auto pressedButton = LayoutButtons(appState);
    if (pressedButton)
        appState.CalculatorState.OnCalculatorButton(pressedButton.value());
    HandleComputerKeyboard(appState.CalculatorState);
}


int main(int, char **)
{
    AppState appState;
    HelloImGui::RunnerParams params;
    params.appWindowParams.windowGeometry.size = { 350, 600 };
    params.callbacks.ShowGui = [&appState]() {  ShowGui(appState); };

    // Fonts loading
    params.callbacks.LoadAdditionalFonts = [&appState]()
    {
        appState.ButtonFont = HelloImGui::LoadFontTTF("fonts/Roboto/Roboto-Bold.ttf", 18.f);
        appState.SmallFont = HelloImGui::LoadFontTTF("fonts/Roboto/Roboto-Bold.ttf", 12.f);
        appState.LDCFont = HelloImGui::LoadFontTTF("fonts/scientific-calculator-lcd-font/ScientificCalculatorLcdRegular-Kn7X.ttf", 15.f);
    };

    // Serialization
    params.callbacks.PostInit = [&appState]()
    {
        return;
        std::string stateSerialized = HelloImGui::LoadUserPref("CalculatorState");
        if (stateSerialized.empty())
            return;
        auto j  = nlohmann::json::parse(stateSerialized, nullptr, false);

        if (!j.is_null())
            appState.CalculatorState.from_json(j);
        else
            printf("Failed to load calculator state from user pref\n");
    };
    params.callbacks.BeforeExit = [&appState]()
    {
        auto j = appState.CalculatorState.to_json();
        std::string stateSerialized = j.dump();
        HelloImGui::SaveUserPref("CalculatorState", stateSerialized);
    };

    // Go!
    HelloImGui::Run(params);
    return 0;
}
