#include "rpn_calculator.h"


namespace RpnCalculator
{
    CalculatorLayoutDefinition::CalculatorLayoutDefinition()
    {
        Buttons = {
            {   { "Inv", ButtonType::Inv},
                { "Deg", ButtonType::DegRadGrad, "To Deg"},
                { "Rad", ButtonType::DegRadGrad, "To Rad"},
                { "Grad", ButtonType::DegRadGrad, "To Grad"}},


            {   { "Pi", ButtonType::DirectNumber },
                { "sin", ButtonType::UnaryOperator, "sin^-1" },
                { "cos", ButtonType::UnaryOperator, "cos^-1" },
                { "tan", ButtonType::UnaryOperator, "tan^-1" }},


            {   { "1/x", ButtonType::UnaryOperator },
                { "log", ButtonType::UnaryOperator, "10^x" },
                { "ln", ButtonType::UnaryOperator },
                { "e^x", ButtonType::UnaryOperator }},

            {   { "sqrt", ButtonType::UnaryOperator },
                { "x^2", ButtonType::UnaryOperator },
                { "floor", ButtonType::UnaryOperator },
                { "y^x", ButtonType::BinaryOperator }},

            {   { "Sto", ButtonType::StackOperator },
                { "Recall", ButtonType::StackOperator },
                { "Roll", ButtonType::StackOperator },
                { "Undo", ButtonType::StackOperator }},

            {   { "Swap", ButtonType::StackOperator },
                { "Dup", ButtonType::StackOperator },
                { "Drop", ButtonType::StackOperator },
                { "Clear", ButtonType::StackOperator }},

            {   { "Enter", ButtonType::Enter}, // "Undo"},
                { "E", ButtonType::Digit},
                { "<=", ButtonType::Backspace }},

            {   { "7", ButtonType::Digit },
                { "8", ButtonType::Digit },
                { "9", ButtonType::Digit },
                { "/", ButtonType::BinaryOperator }},

            {   { "4", ButtonType::Digit },
                { "5", ButtonType::Digit },
                { "6", ButtonType::Digit },
                { "*", ButtonType::BinaryOperator }},

            {   { "1", ButtonType::Digit },
                { "2", ButtonType::Digit },
                { "3", ButtonType::Digit },
                { "-", ButtonType::BinaryOperator }},

            {   { "0", ButtonType::Digit},
                { ".", ButtonType::Digit },
                { "+/-", ButtonType::Digit },
                { "+", ButtonType::BinaryOperator }},
        };
    }



    CalculatorButtonWithInverse::CalculatorButtonWithInverse(
        const std::string& label,
        ButtonType type,
        const std::string& inverseLabel,
        std::optional<ButtonType> inverseType)
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

    const CalculatorButton& CalculatorButtonWithInverse::GetCurrentButton(bool inverseMode) const
    {
        if (inverseMode && InverseButton.has_value())
        return InverseButton.value();
        else
        return Button;
    }

    std::string to_string(AngleUnitType t) {
        switch (t) {
            case AngleUnitType::Deg: return "Deg";
            case AngleUnitType::Rad: return "Rad";
            case AngleUnitType::Grad: return "Grad";
        }
        return "";
    }


// Helper functions for AngleUnit enum since it's not directly supported by nlohmann/json
    NLOHMANN_JSON_SERIALIZE_ENUM( AngleUnitType, {
        {AngleUnitType::Deg, "Deg"},
        {AngleUnitType::Rad, "Rad"},
        {AngleUnitType::Grad, "Grad"},
    })


    //
    //  CalculatorState implementation
    //

    bool CalculatorState::_stackInput()
    {
        if (Input.empty())
            return true;

        bool success = false;

        {
            std::istringstream iss(Input);
            double v;
            iss >> v;

            if (!iss.fail() && iss.eof())
            {
                Stack.store_undo();
                Stack.push_back(v);
                success = true;
            }
            else
                ErrorMessage = "Invalid input";
        }

        Input = "";
        return success;
    }

    void CalculatorState::_onEnter()
    {
        (void)_stackInput();
    }

    void CalculatorState::_onDirectNumber(const std::string& label)
    {
        if (label == "Pi")
            Input += "3.1415926535897932384626433832795";
        else if (label == "e")
            Input += "2.7182818284590452353602874713527";
    }

    void CalculatorState::_onStackOperator(const std::string& cmd)
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
        else if (cmd == "Undo")
        {
            Stack.undo();
        }
        else if (cmd == "Sto")
        {
            if (!Input.empty())
            {
                if (!_stackInput())
                {
                    ErrorMessage = "Invalid number";
                    return;
                }
                StoredValue = Stack.back();
                Stack.pop_back();
            }
            else
            {
                if (Stack.empty())
                {
                    ErrorMessage = "Not enough values on the stack";
                    return;
                }
                StoredValue = Stack.back();
            }
        }
        else if (cmd == "Recall")
        {
            Stack.store_undo();
            Stack.push_back(StoredValue);
        }
        else if (cmd == "Roll")
        {
            if (Stack.empty())
            {
                ErrorMessage = "Not enough values on the stack";
                return;
            }
            Stack.store_undo();
            double a = Stack.back();
            Stack.pop_back();
            Stack.push_front(a);
        }
    }

    void CalculatorState::_onBinaryOperator(const std::string& cmd)
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

    double CalculatorState::_toRadian(double v) const
    {
        if (AngleUnit == AngleUnitType::Deg)
            return v * 3.1415926535897932384626433832795 / 180.;
        else if (AngleUnit == AngleUnitType::Grad)
            return v * 3.1415926535897932384626433832795 / 200.;
        else
            return v;
    }

    double CalculatorState::_toCurrentAngleUnit(double radian) const
    {
        if (AngleUnit == AngleUnitType::Deg)
            return radian * 180. / 3.1415926535897932384626433832795;
        else if (AngleUnit == AngleUnitType::Grad)
            return radian * 200. / 3.1415926535897932384626433832795;
        else
            return radian;
    }

    void CalculatorState::_onUnaryOperator(const std::string& cmd)
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

    void CalculatorState::_onBackspace()
    {
        if (!Input.empty())
            Input.pop_back(); // Remove last input character
    }

    void CalculatorState::_onDegRadGrad(const std::string& cmd)
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

    void CalculatorState::_onInverse()
    {
        InverseMode = !InverseMode;
    }

    void CalculatorState::_onPlusMinus()
    {
        if (Input.empty())
        {
            if (Stack.empty())
            {
                ErrorMessage = "Not enough values on the stack";
                return;
            }
            Stack.store_undo();
            double a = Stack.back();
            Stack.pop_back();
            Stack.push_back(-a);
        }
        else
        {
            if (Input[0] == '-')
                Input = Input.substr(1);
            else
                Input = "-" + Input;
        }
    }

    void CalculatorState::_onDigit(const std::string& digit)
    {
        if (digit == "+/-")
            _onPlusMinus();
        else
            Input += digit;
    }

    void CalculatorState::OnButton(const CalculatorButton& button)
    {
        ErrorMessage = "";

        if (button.Type == ButtonType::Digit)
            _onDigit(button.Label);
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
    nlohmann::json CalculatorState::to_json() const
    {
        nlohmann::json j;
        j["Stack"] = Stack.to_json();
        j["Input"] = Input;
        j["ErrorMessage"] = ErrorMessage;
        j["InverseMode"] = InverseMode;
        j["AngleUnit"] = AngleUnit;

        return j;
    }

    // Deserialization
    void CalculatorState::from_json(const nlohmann::json& j)
    {
        Stack.from_json(j["Stack"]);
        Input = j["Input"].get<std::string>();
        ErrorMessage = j["ErrorMessage"].get<std::string>();
        InverseMode = j["InverseMode"].get<bool>();
        AngleUnit = j["AngleUnit"].get<AngleUnitType>();
    }

}