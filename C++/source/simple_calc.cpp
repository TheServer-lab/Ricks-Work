// simple_calc.cpp — Simple calculator using SoftGUI
#include "softgui_win.hpp"
#include <string>
#include <vector>
#include <stack>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <cctype>

using namespace SoftGUI;

// ---- simple expression evaluator (shunting-yard -> RPN -> eval) ----
static bool is_op_char(char c) { return c=='+'||c=='-'||c=='*'||c=='/'||c=='^'; }
static int prec(char op) {
    if (op=='+'||op=='-') return 1;
    if (op=='*'||op=='/') return 2;
    if (op=='^') return 3;
    return 0;
}

double eval_expression(const std::string &expr, bool &ok, std::string &err) {
    ok = false; err.clear();
    // tokenize to RPN
    std::vector<std::string> output;
    std::stack<char> ops;
    size_t i = 0;
    auto push_num = [&](const std::string &num){ output.push_back(num); };
    while (i < expr.size()) {
        char c = expr[i];
        if (isspace((unsigned char)c)) { ++i; continue; }
        if ( (c>='0' && c<='9') || c=='.' ) {
            std::string num;
            while (i < expr.size() && ( (expr[i]>='0' && expr[i]<='9') || expr[i]=='.' || expr[i]=='e' || expr[i]=='E' ||
                    ((expr[i]=='+'||expr[i]=='-') && !num.empty() && (num.back()=='e' || num.back()=='E')) ) ) {
                num.push_back(expr[i++]);
            }
            push_num(num);
            continue;
        } else if (is_op_char(c)) {
            // unary minus handling: if '-' and it's start or after '(' or another operator -> treat as unary by pushing 0
            if (c=='-' && (output.empty() && ops.empty() && (i==0 || expr[i-1]=='(') )) {
                // unary at start: push 0 then minus
                push_num("0");
            } else if (c=='-' && (i==0 || expr[i-1]=='(' || is_op_char(expr[i-1]))) {
                push_num("0");
            }
            while (!ops.empty() && is_op_char(ops.top()) &&
                ( (prec(ops.top()) > prec(c)) || (prec(ops.top()) == prec(c) && c != '^') ) ) {
                output.push_back(std::string(1, ops.top())); ops.pop();
            }
            ops.push(c); ++i;
        } else if (c == '(') {
            ops.push(c); ++i;
        } else if (c == ')') {
            bool found = false;
            while (!ops.empty()) {
                char t = ops.top(); ops.pop();
                if (t == '(') { found = true; break; }
                output.push_back(std::string(1,t));
            }
            if (!found) { err = "Mismatched parenthesis"; return 0.0; }
            ++i;
        } else {
            err = std::string("Unexpected char: ") + c;
            return 0.0;
        }
    }
    while (!ops.empty()) {
        char t = ops.top(); ops.pop();
        if (t == '(' || t == ')') { err = "Mismatched parenthesis"; return 0.0; }
        output.push_back(std::string(1,t));
    }

    // evaluate RPN
    std::stack<double> st;
    for (auto &tk : output) {
        if (tk.empty()) continue;
        if (tk.size() == 1 && is_op_char(tk[0])) {
            if (st.size() < 2) { err = "Syntax error"; return 0.0; }
            double b = st.top(); st.pop();
            double a = st.top(); st.pop();
            double r = 0;
            switch (tk[0]) {
                case '+': r = a + b; break;
                case '-': r = a - b; break;
                case '*': r = a * b; break;
                case '/': if (b == 0.0) { err = "Division by zero"; return 0.0; } r = a / b; break;
                case '^': r = std::pow(a, b); break;
                default: err = "Unknown op"; return 0.0;
            }
            st.push(r);
        } else {
            // number
            try {
                size_t pos = 0;
                double v = std::stod(tk, &pos);
                if (pos == 0) { err = std::string("Bad number: ") + tk; return 0.0; }
                st.push(v);
            } catch (...) { err = std::string("Bad number: ") + tk; return 0.0; }
        }
    }
    if (st.size() != 1) { err = "Syntax error"; return 0.0; }
    ok = true;
    return st.top();
}

// ---- UI ----
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    Window win(360, 520, "Simple Calculator");

    // expression entry (top)
    auto exprEntry = win.make_entry("");
    exprEntry->geom.x = 12; exprEntry->geom.y = 12; exprEntry->geom.w = 336; exprEntry->geom.h = 36;
    win.place(exprEntry, exprEntry->geom.x, exprEntry->geom.y);

    // result label
    auto resultLbl = win.make_label("0");
    resultLbl->geom.x = 12; resultLbl->geom.y = 56; resultLbl->geom.w = 336; resultLbl->geom.h = 48;
    resultLbl->font_size = 16;
    win.place(resultLbl, resultLbl->geom.x, resultLbl->geom.y);

    // button grid (4 columns x 5 rows)
    std::vector<std::vector<std::string>> grid = {
        {"7","8","9","/"},
        {"4","5","6","*"},
        {"1","2","3","-"},
        {"0",".","=","+"},
        {"C","⌫","("," )"}
    };

    // adapt last row spacing for parentheses (fix tokens)
    grid[4][3] = ")";

    // function to append text to entry
    auto append_to_entry = [&](const std::string &s) {
        exprEntry->text += s;
        exprEntry->mark_dirty();
    };
    auto set_entry = [&](const std::string &s){
        exprEntry->text = s;
        exprEntry->mark_dirty();
    };

    // create buttons
    int btnW = 80, btnH = 56;
    int startX = 12, startY = 116;
    std::vector<std::shared_ptr<Button>> btns;
    for (int r=0;r<(int)grid.size();++r) {
        for (int c=0;c<(int)grid[r].size();++c) {
            std::string label = grid[r][c];
            auto b = win.make_button(label);
            int x = startX + c * (btnW + 8);
            int y = startY + r * (btnH + 8);
            b->geom.x = x; b->geom.y = y; b->geom.w = btnW; b->geom.h = btnH;
            win.place(b, x, y);
            btns.push_back(b);
            // assign click
            b->onclick0 = [label, &exprEntry, &resultLbl, &append_to_entry, &set_entry]() {
                if (label == "C") {
                    exprEntry->text.clear();
                    resultLbl->text = "0";
                    exprEntry->mark_dirty(); resultLbl->mark_dirty();
                } else if (label == "⌫") {
                    if (!exprEntry->text.empty()) {
                        exprEntry->text.pop_back();
                        exprEntry->mark_dirty();
                    }
                } else if (label == "=") {
                    bool ok=false; std::string err;
                    double v = eval_expression(exprEntry->text, ok, err);
                    if (!ok) {
                        resultLbl->text = std::string("Error: ") + (err.empty() ? "Invalid" : err);
                    } else {
                        std::ostringstream ss; ss<<std::fixed<<std::setprecision(10)<<v;
                        std::string s = ss.str();
                        // trim trailing zeros + decimal
                        if (s.find('.')!=std::string::npos) {
                            while (!s.empty() && s.back()=='0') s.pop_back();
                            if (!s.empty() && s.back()=='.') s.pop_back();
                        }
                        resultLbl->text = s;
                    }
                    resultLbl->mark_dirty();
                } else {
                    append_to_entry(label);
                }
            };
        }
    }

    // allow pressing Enter in entry to compute
    exprEntry->on_key = [&exprEntry, &resultLbl](Widget*, char ch){
        if (ch == '\r') {
            bool ok=false; std::string err;
            double v = eval_expression(exprEntry->text, ok, err);
            if (!ok) {
                resultLbl->text = std::string("Error: ") + (err.empty() ? "Invalid" : err);
            } else {
                std::ostringstream ss; ss<<std::fixed<<std::setprecision(10)<<v;
                std::string s = ss.str();
                if (s.find('.')!=std::string::npos) {
                    while (!s.empty() && s.back()=='0') s.pop_back();
                    if (!s.empty() && s.back()=='.') s.pop_back();
                }
                resultLbl->text = s;
            }
            resultLbl->mark_dirty();
        }
    };

    // simple keyboard mapping for convenience
    // rely on Window's WM_CHAR -> focused entry to receive keys, but we also support global shortcuts via buttons below:
    // (optional) map Escape to clear by hooking a focused-entry on_key? We'll keep it simple.

    win.mainloop();
    return 0;
}
