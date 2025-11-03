#include "softgui_win.hpp"
#include <string>
#include <sstream>
#include <cmath>

using namespace SoftGUI;

int main() {
    Window win(300, 400, "SoftGUI Calculator");

    auto display = win.make_entry("");
    display->geom.w = 260;
    display->geom.h = 40;
    win.place(display, 20, 20);

    std::string current_input;
    char last_op = 0;
    double stored_value = 0.0;
    bool new_input = true;

    auto make_button = [&](const std::string &txt, int x, int y, int w = 60, int h = 40) {
        auto b = win.make_button(txt);
        b->geom.x = x; b->geom.y = y; b->geom.w = w; b->geom.h = h;
        win.add_child(b);
        return b;
    };

    auto update_display = [&]() {
        display->text = current_input;
        display->mark_dirty();
    };

    auto press_digit = [&](char d) {
        if (new_input) { current_input = ""; new_input = false; }
        current_input += d;
        update_display();
    };

    auto press_op = [&](char op) {
        double val = current_input.empty() ? 0.0 : std::stod(current_input);
        if (last_op) {
            if (last_op == '+') stored_value += val;
            else if (last_op == '-') stored_value -= val;
            else if (last_op == '*') stored_value *= val;
            else if (last_op == '/') stored_value /= val;
        } else {
            stored_value = val;
        }
        last_op = op;
        new_input = true;
        std::ostringstream ss; ss << stored_value;
        current_input = ss.str();
        update_display();
    };

    auto press_equals = [&]() {
        press_op(last_op);
        last_op = 0;
        new_input = true;
    };

    auto press_clear = [&]() {
        current_input = "";
        stored_value = 0.0;
        last_op = 0;
        new_input = true;
        update_display();
    };

    // Digit buttons
    int start_x = 20, start_y = 80;
    for (int i = 1; i <= 9; ++i) {
        int row = (i-1)/3, col = (i-1)%3;
        auto b = make_button(std::to_string(i), start_x + col*70, start_y + row*50);
        b->onclick0 = [i,&press_digit](){ press_digit('0'+i); };
    }
    auto zero = make_button("0", start_x + 70, start_y + 3*50);
    zero->onclick0 = [&](){ press_digit('0'); };

    auto dot = make_button(".", start_x, start_y + 3*50);
    dot->onclick0 = [&](){ press_digit('.'); };

    // Operator buttons
    auto add = make_button("+", start_x + 210, start_y);
    add->onclick0 = [&](){ press_op('+'); };
    auto sub = make_button("-", start_x + 210, start_y + 50);
    sub->onclick0 = [&](){ press_op('-'); };
    auto mul = make_button("*", start_x + 210, start_y + 100);
    mul->onclick0 = [&](){ press_op('*'); };
    auto div = make_button("/", start_x + 210, start_y + 150);
    div->onclick0 = [&](){ press_op('/'); };
    auto eq = make_button("=", start_x + 210, start_y + 200);
    eq->onclick0 = [&](){ press_equals(); };

    auto clr = make_button("C", start_x + 140, start_y + 200);
    clr->onclick0 = [&](){ press_clear(); };

    win.mainloop();
    return 0;
}
