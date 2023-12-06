// A Simple RPN calculator application
#include "immapp/immapp.h"
#include "imgui.h"

#include <deque>
#include <vector>
#include <map>
#include <optional>


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
    ButtonType  Type;
    std::string InverseLabel = "";
    bool        IsDoubleWidth = false;
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
    std::vector<CalculatorButton> Buttons = {
        { "Inv", ButtonType::Inv},
        { "Deg", ButtonType::DegRadGrad},
        { "Rad", ButtonType::DegRadGrad},
        { "Grad", ButtonType::DegRadGrad},

        { "Pi", ButtonType::DirectNumber },
        { "sin", ButtonType::UnaryOperator, "sin^-1" },
        { "cos", ButtonType::UnaryOperator, "cos^-1" },
        { "tan", ButtonType::UnaryOperator, "tan^-1" },

        { "1/x", ButtonType::UnaryOperator },
        { "log", ButtonType::UnaryOperator, "10^x" },
        { "ln", ButtonType::UnaryOperator },
        { "e^x", ButtonType::UnaryOperator },

        { "sqrt", ButtonType::UnaryOperator },
        { "x^2", ButtonType::UnaryOperator },
        { "floor", ButtonType::UnaryOperator },
        { "y^x", ButtonType::BinaryOperator },

        { "Swap", ButtonType::StackOperator },
        { "Dup", ButtonType::StackOperator },
        { "Drop", ButtonType::StackOperator },
        { "Clear", ButtonType::StackOperator },

        { "Enter", ButtonType::Enter, "", true},
        { "E", ButtonType::Digit},
        { "<=", ButtonType::Backspace },

        { "7", ButtonType::Digit },
        { "8", ButtonType::Digit },
        { "9", ButtonType::Digit },
        { "/", ButtonType::BinaryOperator },

        { "4", ButtonType::Digit },
        { "5", ButtonType::Digit },
        { "6", ButtonType::Digit },
        { "*", ButtonType::BinaryOperator },

        { "1", ButtonType::Digit },
        { "2", ButtonType::Digit },
        { "3", ButtonType::Digit },
        { "-", ButtonType::BinaryOperator },

        { "0", ButtonType::Digit},
        { ".", ButtonType::Digit },
        { "+/-", ButtonType::Digit },
        { "+", ButtonType::BinaryOperator },
    };

    int DisplayedStackSize = 4;
    int NbDecimals = 16;
    int NbButtonsRows = 10;
};


struct CalculatorState
{
    enum class AngleUnit
    {
        Deg, Rad, Grad
    };

    std::deque<double> Stack;
    std::string Input;
    std::string ErrorMessage;
    bool InverseMode = false;
    CalculatorLayoutDefinition CalculatorLayoutDefinition;
    AngleUnit AngleUnit = AngleUnit::Deg;

    bool _StackInput()
    {
        if (Input.empty())
            return true;

        bool success = false;

        try {
            double v = std::stod(Input);
            Stack.push_back(v);
            success = true;
        }
        catch (std::invalid_argument&) { ErrorMessage = "Invalid number"; }
        catch (std::out_of_range&) { ErrorMessage = "Out of range"; }

        Input = "";
        return success;
    }

    void _OnDirectNumber(const std::string& label)
    {
        if (label == "Pi")
            Input += "3.1415926535897932384626433832795";
        else if (label == "e")
            Input += "2.7182818284590452353602874713527";
    }

    void _OnStackOperator(const std::string& cmd)
    {
        if (cmd == "Swap")
        {
            if (Stack.size() < 2)
            {
                ErrorMessage = "Not enough values on the stack";
                return;
            }
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
            Stack.pop_back();
        }
        else if (cmd == "Clear")
        {
            Stack.clear();
        }
    }

    void _OnBinaryOperator(const std::string& cmd)
    {
        if (!_StackInput())
            return;
        if (Stack.size() < 2)
        {
            ErrorMessage = "Not enough values on the stack";
            return;
        }
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

    double _ToRadian(double v)
    {
        if (AngleUnit == AngleUnit::Deg)
            return v * 3.1415926535897932384626433832795 / 180.;
        else if (AngleUnit == AngleUnit::Grad)
            return v * 3.1415926535897932384626433832795 / 200.;
        else
            return v;
    }

    double _ToCurrentAngleUnit(double radian)
    {
        if (AngleUnit == AngleUnit::Deg)
            return radian * 180. / 3.1415926535897932384626433832795;
        else if (AngleUnit == AngleUnit::Grad)
            return radian * 200. / 3.1415926535897932384626433832795;
        else
            return radian;
    }

    void _OnUnaryOperator(const std::string& cmd)
    {
        if (!_StackInput())
            return;
        if (Stack.empty())
        {
            ErrorMessage = "Not enough values on the stack";
            return;
        }
        double a = Stack.back();
        Stack.pop_back();
        if (cmd == "sin")
            Stack.push_back(sin(_ToRadian(a)));
        else if (cmd == "cos")
            Stack.push_back(cos(_ToRadian(a)));
        else if (cmd == "tan")
            Stack.push_back(tan(_ToRadian(a)));
        else if (cmd == "sin^-1")
            Stack.push_back(_ToCurrentAngleUnit(asin(a)));
        else if (cmd == "cos^-1")
            Stack.push_back(_ToCurrentAngleUnit(acos(a)));
        else if (cmd == "tan^-1")
            Stack.push_back(_ToCurrentAngleUnit(atan(a)));
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

    void _OnBackspace()
    {
        if (!Input.empty())
            Input.pop_back(); // Remove last input character
    }

    void _OnDegRadGrad(const std::string& cmd)
    {
        if (cmd == "Deg")
            AngleUnit = AngleUnit::Deg;
        else if (cmd == "Rad")
            AngleUnit = AngleUnit::Rad;
        else if (cmd == "Grad")
            AngleUnit = AngleUnit::Grad;
    }

    void _OnInverse()
    {
        InverseMode = !InverseMode;
    }

    void OnButton(const CalculatorButton& button)
    {
        ErrorMessage = "";

        if (button.Type == ButtonType::Digit)
            Input += button.Label;
        else if (button.Type == ButtonType::Backspace)
            _OnBackspace();
        else if (button.Type == ButtonType::DirectNumber)
            _OnDirectNumber(button.Label);
        else if (button.Type == ButtonType::Enter)
            _StackInput();
        else if (button.Type == ButtonType::StackOperator)
            _OnStackOperator(button.Label);
        else if (button.Type == ButtonType::BinaryOperator)
            _OnBinaryOperator(button.Label);
        else if (button.Type == ButtonType::UnaryOperator)
            _OnUnaryOperator(button.Label);
        else if (button.Type == ButtonType::DegRadGrad)
            _OnDegRadGrad(button.Label);
        else if (button.Type == ButtonType::Inv)
            _OnInverse();
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
    ImFont* ButtonFont, *DisplayFont, *MessageFont;
    CalculatorState CalculatorState;
};


// Draw a button with a size that fits 4 buttons per row and a color based on the button type
bool DrawOneButton(const CalculatorButton& button, ImVec2 standardSize, ImVec2 doubleButtonSize)
{

    ImVec2 buttonSize =  button.IsDoubleWidth ? doubleButtonSize : standardSize;
    ImVec4 color = ButtonColors[button.Type];
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    bool pressed = ImGui::Button(button.Label.c_str(), buttonSize);
    ImGui::PopStyleColor();
    return pressed;

}


// Layout 4 buttons per row (with possible double width)
// Returns the label of the pressed button
std::optional<CalculatorButton> LayoutButtons(AppState& appState)
{
    ImVec2 standardButtonSize, doubleButtonSize;
    {
        ImVec2 spacing = ImGui::GetStyle().ItemSpacing;

        float buttonWidth = (ImGui::GetWindowWidth() - spacing.x * 3.f) / 4.f;
        float remainingHeight = ImGui::GetWindowHeight() - ImGui::GetCursorPosY();
        float buttonHeight =
            remainingHeight / (float)appState.CalculatorState.CalculatorLayoutDefinition.NbButtonsRows - spacing.y;

        standardButtonSize = ImVec2(buttonWidth, buttonHeight);
        doubleButtonSize = ImVec2(
            standardButtonSize.x * 2 + ImGui::GetStyle().ItemSpacing.x,
            standardButtonSize.y);
    }


    std::optional<CalculatorButton> pressedButton;
    int xPos = 1;
    for (const auto& button: appState.CalculatorState.CalculatorLayoutDefinition.Buttons)
    {
        if (DrawOneButton(button, standardButtonSize, doubleButtonSize))
            pressedButton = button;
        xPos += button.IsDoubleWidth ? 2 : 1;
        if (xPos > 4)
            xPos = 1;
        else
            ImGui::SameLine();
    }
    return pressedButton;
}

void GuiDisplay(AppState& appState)
{
    const auto& calculatorState = appState.CalculatorState;

    // Display the stack
    ImGui::PushFont(appState.DisplayFont);
    int nbViewableStackLines = calculatorState.CalculatorLayoutDefinition.DisplayedStackSize;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.75f, 0.75f, 0.75f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 1.f));
    if (ImGui::BeginChild("StackDisplay", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY))
    {
        ImGui::PushFont(appState.MessageFont);

        // Display angle unit
        std::string angleUnitStr = (calculatorState.AngleUnit == CalculatorState::AngleUnit::Deg ? "Deg" :
            calculatorState.AngleUnit == CalculatorState::AngleUnit::Rad ? "Rad" : "Grad");
        ImGui::SameLine(int(calculatorState.AngleUnit) * ImmApp::EmSize(2.f));
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
            int stackIndex = calculatorState.Stack.size() - nbViewableStackLines + i;
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


int main(int argc, char *argv[])
{
    AppState appState;
    HelloImGui::RunnerParams params;
    params.appWindowParams.windowGeometry.size = { 400, 600 };
    params.callbacks.ShowGui = [&appState]() {  ShowGui(appState); };
    params.callbacks.LoadAdditionalFonts = [&appState]() {
        appState.ButtonFont = HelloImGui::LoadFontTTF("fonts/Roboto/Roboto-Bold.ttf", 18.f);
        appState.MessageFont = HelloImGui::LoadFontTTF("fonts/Roboto/Roboto-Bold.ttf", 12.f);
        appState.DisplayFont = HelloImGui::LoadFontTTF("fonts/scientific-calculator-lcd-font/ScientificCalculatorLcdRegular-Kn7X.ttf", 15.f);
    };

    HelloImGui::Run(params);
    return 0;
}