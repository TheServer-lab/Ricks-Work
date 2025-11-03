#include "easycpp.hpp"

int main() {
    // Create main window
    Window win(320, 480, "EasyCPP Calculator");

    // Display label for showing result
    auto display = win.make_label("0");
    display->geom = {20, 20, 280, 50};
    display->font_size = 20;

    // Variables for calculator logic
    string current = "";
    double lastValue = 0.0;
    char op = 0;
    bool newInput = false;

    auto update_display = [&]() {
        display->text = current.empty() ? "0" : current;
        display->mark_dirty();
    };

    auto handle_number = [&](string num) {
        if (newInput) { current = ""; newInput = false; }
        current += num;
        update_display();
    };

    auto handle_operator = [&](char oper) {
        if (!current.empty()) {
            lastValue = stod(current);
            current.clear();
        }
        op = oper;
        newInput = false;
        update_display();
    };

    auto calculate = [&]() {
        if (current.empty() || op == 0) return;
        double val2 = stod(current);
        double result = lastValue;

        switch (op) {
            case '+': result += val2; break;
            case '-': result -= val2; break;
            case '*': result *= val2; break;
            case '/': 
                if (val2 != 0) result /= val2; 
                else { display->text = "Error"; display->mark_dirty(); return; }
                break;
        }

        current = to_string(result);
        // trim trailing .000000
        if (current.find('.') != string::npos)
            current.erase(current.find_last_not_of('0') + 1, string::npos);
        if (current.back() == '.') current.pop_back();

        update_display();
        op = 0;
        newInput = true;
    };

    auto clear_all = [&]() {
        current = "";
        lastValue = 0;
        op = 0;
        update_display();
    };

    // Create buttons
    vector<string> labels = {
        "7","8","9","/",
        "4","5","6","*",
        "1","2","3","-",
        "0",".","=","+",
        "C"
    };

    int x = 20, y = 90;
    int w = 60, h = 50;
    int margin = 10;
    int count = 0;

    for (auto &lbl : labels) {
        auto btn = win.make_button(lbl);
        btn->geom = {x, y, w, h};
        btn->onclick0 = [&, lbl]() {
            if (lbl == "C") clear_all();
            else if (lbl == "=") calculate();
            else if (lbl == "+" || lbl == "-" || lbl == "*" || lbl == "/") handle_operator(lbl[0]);
            else handle_number(lbl);
        };
        x += w + margin;
        count++;
        if (count % 4 == 0) { x = 20; y += h + margin; }
    }

    // Run main loop
    win.mainloop();
    return 0;
}
