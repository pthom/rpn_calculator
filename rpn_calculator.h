#pragma once
#include <string>
#include <deque>
#include <stack>
#include <sstream>
#include "nlohmann_json.hpp"



namespace RpnCalculator
{

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

        CalculatorButtonWithInverse(
            const std::string& label,
            ButtonType type,
            const std::string& inverseLabel = "",
            std::optional<ButtonType> inverseType = std::nullopt);

        const CalculatorButton& GetCurrentButton(bool inverseMode) const;
    };



    struct CalculatorLayoutDefinition
    {
        /*
        Here is how the calculator looks like:
            * each button row has 4 buttons
            * Enter is a special button that is double width
            * The stack and error indicator is displayed on top
            * Some buttons have an inverse mode
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
        CalculatorLayoutDefinition();
        std::vector<std::vector<CalculatorButtonWithInverse>> Buttons;
        int DisplayedStackSize = 4;
        int NbButtonsPerRow = 4;
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
        void push_front(double v) { Stack.push_front(v); }
        void pop_back() { Stack.pop_back(); }
        void clear() { Stack.clear(); }

        void undo() { if (!_undoStack.empty()) { Stack = _undoStack.top(); _undoStack.pop(); } }
        void store_undo() {_undoStack.push(Stack); }

        // Serialization
        nlohmann::json to_json() const {  nlohmann::json j; j["Stack"] = Stack; return j; }
        void from_json(const nlohmann::json& j) { Stack = j["Stack"].get<std::deque<double>>(); }
    };


    enum class AngleUnitType
    {
        Deg, Rad, Grad
    };
    std::string to_string(AngleUnitType t);


    class CalculatorState
    {
    public:
        // public members (displayed by the application)
        CalculatorLayoutDefinition CalculatorLayoutDefinition;
        bool InverseMode = false;
        AngleUnitType AngleUnit = AngleUnitType::Deg;
        std::string Input;
        std::string ErrorMessage;
        double StoredValue = 0.;
        UndoableNumberStack Stack;

        // callback
        void OnButton(const CalculatorButton& button);

        // serialization
        nlohmann::json to_json() const;
        void from_json(const nlohmann::json& j);

    private:
        // private callback helpers
        bool _stackInput();
        void _onEnter();
        void _onDirectNumber(const std::string& label);
        void _onStackOperator(const std::string& cmd);
        void _onBinaryOperator(const std::string& cmd);
        void _onUnaryOperator(const std::string& cmd);
        void _onBackspace();
        void _onDegRadGrad(const std::string& cmd);
        void _onInverse();
        void _onPlusMinus();
        void _onDigit(const std::string& digit);

        // private angle helpers
        double _toRadian(double v) const;
        double _toCurrentAngleUnit(double radian) const;
    };

}
