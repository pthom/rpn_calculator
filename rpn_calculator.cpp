// A Simple RPN calculator application
#define IMGUI_DEFINE_MATH_OPERATORS
#include "immapp/immapp.h"
#include "imgui.h"
#include "imgui_internal.h"

#include "nlohmann_json.hpp"
#include <deque>
#include <stack>
#include <vector>
#include <map>
#include <optional>


using json = nlohmann::json;


enum class ButtonType
{
    Digit,            // 0-9
    DirectNumber,     // Pi, e
    Backspace,        // <=

    BinaryOperator,   // +, -, *
    UnaryOperator,    // sin, cos, tan, log, ln, sqrt, x^2, floor
    StackOperator,    // Swap, Dup, Drop, Clear

    Inv,            // Inverse
    DegRadGrad,     // Degree, Radian, Gradian
    Enter           // Enter
};


struct CalculatorButton
{
    std::string Label;
    ButtonType  Type = ButtonType::Digit;
    bool        IsDoubleWidth = false;
};

struct CalculatorButtonWithInverse
{
    CalculatorButton Button;
    std::optional<CalculatorButton> InverseButton = std::nullopt;

    [[nodiscard]] const CalculatorButton& GetCurrentButton(bool inverseMode) const
    {
        if (inverseMode && InverseButton.has_value())
            return InverseButton.value();
        else
            return Button;
    }

    CalculatorButtonWithInverse(
        const std::string& label,
        ButtonType type,
        const std::string& inverseLabel = "",
        std::optional<ButtonType> inverseType = std::nullopt)
    {
        Button.Label = label;
        Button.Type = type;
        if (Button.Label == "Enter")
            Button.IsDoubleWidth = true;
        if (!inverseLabel.empty())
        {
            InverseButton = CalculatorButton{ inverseLabel, Button.Type };
            if (inverseType.has_value())
                InverseButton->Type = inverseType.value();
            InverseButton->IsDoubleWidth = Button.IsDoubleWidth;
        }
    }
};



struct CalculatorLayoutDefinition
{
    /*
    Here is how the calculator looks like:
        * each button row has 4 buttons
        * Enter is a special button that is double width
        * The stack and error indicator is displayed on top
    ==============================
                        stack N-3
                        stack N-2
                        stack N-1
                          stack N
    user input
                   error indicator
    ==============================
    [Inv]   [Deg]   [Rad]   [Grad]
    [Pi]    [sin]   [cos]   [tan]
    [1/x]   [log]   [ln]    [e^x]
    [sqrt]  [x^2]   [floor] [y^x]
    ==============================
    [Swap]  [Dup]    [Drop] [Clear]
    [   Enter ]      [E]      [<=]
    [7]     [8]      [9]      [/]
    [4]     [5]      [6]      [*]
    [1]     [2]      [3]      [-]
    [0]     [.]      [+/-]    [+]
    ==============================
     */
    std::vector<std::vector<CalculatorButtonWithInverse>> Buttons = {
        {{"Inv", ButtonType::Inv},
         {"Deg", ButtonType::DegRadGrad, "To Deg"},
         {"Rad", ButtonType::DegRadGrad, "To Rad"},
         {"Grad", ButtonType::DegRadGrad, "To Grad"}},

        {{"Pi", ButtonType::DirectNumber },
        { "sin", ButtonType::UnaryOperator, "sin^-1" },
        { "cos", ButtonType::UnaryOperator, "cos^-1" },
        { "tan", ButtonType::UnaryOperator, "tan^-1" }},


        {{"1/x", ButtonType::UnaryOperator },
        { "log", ButtonType::UnaryOperator, "10^x" },
        { "ln", ButtonType::UnaryOperator },
        { "e^x", ButtonType::UnaryOperator }},

        {{"sqrt", ButtonType::UnaryOperator },
        { "x^2", ButtonType::UnaryOperator },
        { "floor", ButtonType::UnaryOperator },
        { "y^x", ButtonType::BinaryOperator }},

        {{"Swap", ButtonType::StackOperator },
        { "Dup", ButtonType::StackOperator },
        { "Drop", ButtonType::StackOperator },
        { "Clear", ButtonType::StackOperator }},

        {{"Enter", ButtonType::Enter, "Undo"},
        { "E", ButtonType::Digit},
        { "<=", ButtonType::Backspace }},

        {{"7", ButtonType::Digit },
        { "8", ButtonType::Digit },
        { "9", ButtonType::Digit },
        { "/", ButtonType::BinaryOperator }},

        {{"4", ButtonType::Digit },
        { "5", ButtonType::Digit },
        { "6", ButtonType::Digit },
        { "*", ButtonType::BinaryOperator }},

        {{"1", ButtonType::Digit },
        { "2", ButtonType::Digit },
        { "3", ButtonType::Digit },
        { "-", ButtonType::BinaryOperator }},

        {{"0", ButtonType::Digit},
        { ".", ButtonType::Digit },
        { "+/-", ButtonType::Digit },
        { "+", ButtonType::BinaryOperator }},
    };

    int DisplayedStackSize = 4;
    int NbDecimals = 12;
};


struct UndoableNumberStack
{
    std::deque<double> Stack;
    std::stack<std::deque<double>> _undoStack;

    size_t size() const { return Stack.size(); }
    bool empty() const { return Stack.empty(); }
    double back() const { return Stack.back();}
    double operator[](int index) const { return Stack[index]; }
    void push_back(double v) { Stack.push_back(v); }
    void pop_back() { Stack.pop_back(); }
    void clear() { Stack.clear(); }

    void undo()
    {
        if (_undoStack.empty())
            return;
        Stack = _undoStack.top();
        _undoStack.pop();
    }

    void store_undo()
    {
        _undoStack.push(Stack);
    }

    // Serialization
    json to_json() const
    {
        json j;
        j["Stack"] = Stack;
        return j;
    }

    // Deserialization
    void from_json(const json& j)
    {
        Stack = j["Stack"].get<std::deque<double>>();
    }
};


enum class AngleUnitType
{
    Deg, Rad, Grad
};

// Helper functions for AngleUnit enum since it's not directly supported by nlohmann/json
NLOHMANN_JSON_SERIALIZE_ENUM( AngleUnitType, {
    {AngleUnitType::Deg, "Deg"},
    {AngleUnitType::Rad, "Rad"},
    {AngleUnitType::Grad, "Grad"},
})

struct CalculatorState
{
    UndoableNumberStack Stack;

    std::string Input;
    std::string ErrorMessage;
    bool InverseMode = false;
    CalculatorLayoutDefinition CalculatorLayoutDefinition;
    AngleUnitType AngleUnit = AngleUnitType::Deg;

    bool _stackInput()
    {
        if (Input.empty())
            return true;

        bool success = false;

        try {
            double v = std::stod(Input);
            Stack.store_undo();
            Stack.push_back(v);
            success = true;
        }
        catch (std::invalid_argument&) { ErrorMessage = "Invalid number"; }
        catch (std::out_of_range&) { ErrorMessage = "Out of range"; }

        Input = "";
        return success;
    }

    void _onEnter()
    {
        if (InverseMode)
            Stack.undo();
        else
            (void)_stackInput();
    }

    void _onDirectNumber(const std::string& label)
    {
        if (label == "Pi")
            Input += "3.1415926535897932384626433832795";
        else if (label == "e")
            Input += "2.7182818284590452353602874713527";
    }

    void _onStackOperator(const std::string& cmd)
    {
        if (cmd == "Swap")
        {
            if (Stack.size() < 2)
            {
                ErrorMessage = "Not enough values on the stack";
                return;
            }
            Stack.store_undo();
            double a = Stack.back();
            Stack.pop_back();
            double b = Stack.back();
            Stack.pop_back();
            Stack.push_back(a);
            Stack.push_back(b);
        }
        else if (cmd == "Dup")
        {
            if (Stack.empty())
            {
                ErrorMessage = "Not enough values on the stack";
                return;
            }
            Stack.store_undo();
            double a = Stack.back();
            Stack.push_back(a);
        }
        else if (cmd == "Drop")
        {
            if (Stack.empty())
            {
                ErrorMessage = "Not enough values on the stack";
                return;
            }
            Stack.store_undo();
            Stack.pop_back();
        }
        else if (cmd == "Clear")
        {
            Stack.store_undo();
            Stack.clear();
        }
    }

    void _onBinaryOperator(const std::string& cmd)
    {
        if (!_stackInput())
            return;
        if (Stack.size() < 2)
        {
            ErrorMessage = "Not enough values on the stack";
            return;
        }
        Stack.store_undo();
        double b = Stack.back();
        Stack.pop_back();
        double a = Stack.back();
        Stack.pop_back();
        if (cmd == "+")
            Stack.push_back(a + b);
        else if (cmd == "-")
            Stack.push_back(a - b);
        else if (cmd == "*")
            Stack.push_back(a * b);
        else if (cmd == "/")
        {
            if (b == 0.)
                ErrorMessage = "Division by zero";
            else
                Stack.push_back(a / b);
        }
        else if (cmd == "y^x")
            Stack.push_back(pow(a, b));
    }

    [[nodiscard]] double _toRadian(double v) const
    {
        if (AngleUnit == AngleUnitType::Deg)
            return v * 3.1415926535897932384626433832795 / 180.;
        else if (AngleUnit == AngleUnitType::Grad)
            return v * 3.1415926535897932384626433832795 / 200.;
        else
            return v;
    }

    [[nodiscard]] double _toCurrentAngleUnit(double radian) const
    {
        if (AngleUnit == AngleUnitType::Deg)
            return radian * 180. / 3.1415926535897932384626433832795;
        else if (AngleUnit == AngleUnitType::Grad)
            return radian * 200. / 3.1415926535897932384626433832795;
        else
            return radian;
    }

    void _onUnaryOperator(const std::string& cmd)
    {
        if (!_stackInput())
            return;
        if (Stack.empty())
        {
            ErrorMessage = "Not enough values on the stack";
            return;
        }
        Stack.store_undo();
        double a = Stack.back();
        Stack.pop_back();
        if (cmd == "sin")
            Stack.push_back(sin(_toRadian(a)));
        else if (cmd == "cos")
            Stack.push_back(cos(_toRadian(a)));
        else if (cmd == "tan")
            Stack.push_back(tan(_toRadian(a)));
        else if (cmd == "sin^-1")
            Stack.push_back(_toCurrentAngleUnit(asin(a)));
        else if (cmd == "cos^-1")
            Stack.push_back(_toCurrentAngleUnit(acos(a)));
        else if (cmd == "tan^-1")
            Stack.push_back(_toCurrentAngleUnit(atan(a)));
        else if (cmd == "1/x")
            Stack.push_back(1. / a);
        else if (cmd == "log")
            Stack.push_back(log10(a));
        else if (cmd == "ln")
            Stack.push_back(log(a));
        else if (cmd == "10^x")
            Stack.push_back(pow(10., a));
        else if (cmd == "e^x")
            Stack.push_back(exp(a));
        else if (cmd == "sqrt")
            Stack.push_back(sqrt(a));
        else if (cmd == "x^2")
            Stack.push_back(a * a);
        else if (cmd == "floor")
            Stack.push_back(floor(a));
    }

    void _onBackspace()
    {
        if (!Input.empty())
            Input.pop_back(); // Remove last input character
    }

    void _onDegRadGrad(const std::string& cmd)
    {
        if (! InverseMode)
        {
            if (cmd == "Deg")
                AngleUnit = AngleUnitType::Deg;
            else if (cmd == "Rad")
                AngleUnit = AngleUnitType::Rad;
            else if (cmd == "Grad")
                AngleUnit = AngleUnitType::Grad;
        }
        else
        {
            if (!_stackInput())
                return;
            if (Stack.empty())
            {
                ErrorMessage = "Not enough values on the stack";
                return;
            }
            Stack.store_undo();
            double a = Stack.back();
            Stack.pop_back();
            if (cmd == "To Deg")
                Stack.push_back(a * 180. / 3.1415926535897932384626433832795);
            else if (cmd == "To Rad")
                Stack.push_back(a * 3.1415926535897932384626433832795 / 180.);
            else if (cmd == "To Grad")
                Stack.push_back(a * 200. / 3.1415926535897932384626433832795);
        }
    }

    void _onInverse()
    {
        InverseMode = !InverseMode;
    }

    void OnButton(const CalculatorButton& button)
    {
        ErrorMessage = "";

        if (button.Type == ButtonType::Digit)
            Input += button.Label;
        else if (button.Type == ButtonType::Backspace)
            _onBackspace();
        else if (button.Type == ButtonType::DirectNumber)
            _onDirectNumber(button.Label);
        else if (button.Type == ButtonType::Enter)
            _onEnter();
        else if (button.Type == ButtonType::StackOperator)
            _onStackOperator(button.Label);
        else if (button.Type == ButtonType::BinaryOperator)
            _onBinaryOperator(button.Label);
        else if (button.Type == ButtonType::UnaryOperator)
            _onUnaryOperator(button.Label);
        else if (button.Type == ButtonType::DegRadGrad)
            _onDegRadGrad(button.Label);
        else if (button.Type == ButtonType::Inv)
            _onInverse();
    }

    // Serialization
    json to_json() const
    {
        json j;
        j["Stack"] = Stack.to_json();
        j["Input"] = Input;
        j["ErrorMessage"] = ErrorMessage;
        j["InverseMode"] = InverseMode;
        j["AngleUnit"] = AngleUnit;

        return j;
    }

    // Deserialization
    void from_json(const json& j)
    {
        Stack.from_json(j["Stack"]);
        Input = j["Input"].get<std::string>();
        ErrorMessage = j["ErrorMessage"].get<std::string>();
        InverseMode = j["InverseMode"].get<bool>();
        AngleUnit = j["AngleUnit"].get<AngleUnitType>();
    }
};




///////  GUI  Below ///////


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
    ImFont* ButtonFont = nullptr, *DisplayFont = nullptr, *MessageFont = nullptr;
    CalculatorState CalculatorState;
};


// Draw a button with a size that fits 4 buttons per row and a color based on the button type
bool DrawOneButton(
    const CalculatorButton& button,
    bool inverseMode,
    ImVec2 standardSize,
    ImVec2 doubleButtonSize,
    ImFont* inverseFont
    )
{
    ImVec2 buttonSize =  button.IsDoubleWidth ? doubleButtonSize : standardSize;
    ImVec4 color = ButtonColors[button.Type];
    if (inverseMode)
        color = ImVec4(0.6f, 0.4f, 0.4f, 1.f);
    ImGui::PushStyleColor(ImGuiCol_Button, color);

    ImVec2 buttonPosition = ImGui::GetCursorScreenPos();

    std::string id = std::string("##") + button.Label;
    bool pressed = ImGui::Button(id.c_str(), buttonSize);

    // Draw the button label (with possible exponent)
    {
        // if the label contains "^" we will need to split in two parts
        bool isExponent = (button.Label.find('^') != std::string::npos);
        std::string labelStd, labelExponent;
        if (!isExponent)
            labelStd = button.Label;
        else
        {
            labelStd = button.Label.substr(0, button.Label.find('^'));
            labelExponent = button.Label.substr(button.Label.find('^') + 1);
        }
        {
            ImVec2 sizeStd = ImGui::CalcTextSize(labelStd.c_str());
            ImGui::PushFont(inverseFont);
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
            ImGui::PushFont(inverseFont);
            ImGui::GetForegroundDrawList()->AddText(
                labelExponentPosition,
                ImGui::GetColorU32(ImGuiCol_Text),
                labelExponent.c_str());
            ImGui::PopFont();

        }
    }

    ImGui::PopStyleColor();
    return pressed;
}


// Layout 4 buttons per row (with possible double width)
// Returns the pressed button
std::optional<CalculatorButton> LayoutButtons(AppState& appState)
{
    ImGui::PushFont(appState.ButtonFont);
    float calculatorBorderMargin = ImmApp::EmSize(0.5f);
    ImGui::GetStyle().ItemSpacing = {calculatorBorderMargin, calculatorBorderMargin};

    const auto& buttonsRows = appState.CalculatorState.CalculatorLayoutDefinition.Buttons;

    // Calculate buttons size so that they fit the remaining space
    ImVec2 standardButtonSize, doubleButtonSize;
    {
        ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
        ImVec2 totalButtonsSpacing(
            spacing.x * (4.f - 1.f),
            spacing.y * ((float)buttonsRows.size() - 1.f)
            );

        float buttonWidth = (ImGui::GetWindowWidth() - totalButtonsSpacing.x - calculatorBorderMargin * 2.f) / 4.f;

        float remainingHeight =
            ImGui::GetWindowHeight()
            - ImGui::GetCursorPosY()
            - totalButtonsSpacing.y
            - calculatorBorderMargin * 2.f;
        float buttonHeight = remainingHeight / (float)buttonsRows.size();

        standardButtonSize = ImVec2(buttonWidth, buttonHeight);
        doubleButtonSize = ImVec2(
            standardButtonSize.x * 2 + ImGui::GetStyle().ItemSpacing.x,
            standardButtonSize.y);
    }

    std::optional<CalculatorButton> pressedButton;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + calculatorBorderMargin);
    for (const auto& buttonRow: buttonsRows)
    {
        ImGui::SetCursorPosX(calculatorBorderMargin);
        for (const auto& buttonWithInverse: buttonRow)
        {
            bool inverseMode = appState.CalculatorState.InverseMode && buttonWithInverse.InverseButton.has_value();
            const CalculatorButton& button = buttonWithInverse.GetCurrentButton(inverseMode);
            if (DrawOneButton(button, inverseMode, standardButtonSize, doubleButtonSize, appState.MessageFont))
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

    ImGui::Spacing();
    ImGui::SetCursorPosX(ImGui::GetStyle().ItemSpacing.x);

    // Display the stack
    ImGui::PushFont(appState.DisplayFont);
    int nbViewableStackLines = calculatorState.CalculatorLayoutDefinition.DisplayedStackSize;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.75f, 0.75f, 0.75f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 1.f));
    if (ImGui::BeginChild("StackDisplay", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY))
    {
        ImGui::PushFont(appState.MessageFont);

        // Display angle unit
        std::string angleUnitStr = (calculatorState.AngleUnit == AngleUnitType::Deg ? "Deg" :
            calculatorState.AngleUnit == AngleUnitType::Rad ? "Rad" : "Grad");
        ImGui::SameLine((float)(int)(calculatorState.AngleUnit) * ImmApp::EmSize(2.f));
        ImGui::Text("%s", angleUnitStr.c_str());

        // Display Inv indicator on the same line,but at the right
        if (calculatorState.InverseMode)
        {
            ImGui::SameLine(ImGui::GetWindowWidth() - ImmApp::EmSize(2.f));
            ImGui::Text("Inv");
        }

        ImGui::PopFont();

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
        ImGui::Text("%s", calculatorState.Input.c_str());
        ImGui::PopFont();

        ImGui::PushFont(appState.MessageFont);
        ImGui::Text("%s", calculatorState.ErrorMessage.c_str());
        ImGui::PopFont();

        ImGui::PopStyleColor(2);
        ImGui::EndChild();
    }
}


void ShowGui(AppState& appState)
{
    GuiDisplay(appState);
    auto pressedButton = LayoutButtons(appState);
    if (pressedButton)
        appState.CalculatorState.OnButton(pressedButton.value());
}


int main(int, char **)
{
    AppState appState;
    HelloImGui::RunnerParams params;
    params.appWindowParams.windowGeometry.size = { 350, 600 };
    params.callbacks.ShowGui = [&appState]() {  ShowGui(appState); };
    params.callbacks.LoadAdditionalFonts = [&appState]() {
        appState.ButtonFont = HelloImGui::LoadFontTTF("fonts/Roboto/Roboto-Bold.ttf", 18.f);
        appState.MessageFont = HelloImGui::LoadFontTTF("fonts/Roboto/Roboto-Bold.ttf", 12.f);
        appState.DisplayFont = HelloImGui::LoadFontTTF("fonts/scientific-calculator-lcd-font/ScientificCalculatorLcdRegular-Kn7X.ttf", 15.f);
    };
    params.callbacks.PostInit = [&appState]() {
        std::string stateSerialized = HelloImGui::LoadUserPref("CalculatorState");
        try {
            auto j = json::parse(stateSerialized);
            appState.CalculatorState.from_json(j);
        }
        catch(std::exception&)
        {
            printf("Failed to load calculator state from user pref\n");
        }
    };
    params.callbacks.BeforeExit = [&appState]() {
        auto j = appState.CalculatorState.to_json();
        std::string stateSerialized = j.dump();
        HelloImGui::SaveUserPref("CalculatorState", stateSerialized);
    };

    HelloImGui::Run(params);
    return 0;
}