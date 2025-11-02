#include "easycpp.hpp"
#include <stack>
#include <cctype>
#include <sstream>

using namespace easy;

// --- Simple expression evaluator (handles + - * / and parentheses) ---
double eval_expr(const std::string& s) {
    std::istringstream in(s);
    std::stack<double> values;
    std::stack<char> ops;

    auto apply = [&](char op) {
        if (values.size() < 2) return;
        double b = values.top(); values.pop();
        double a = values.top(); values.pop();
        if (op == '+') values.push(a + b);
        else if (op == '-') values.push(a - b);
        else if (op == '*') values.push(a * b);
        else if (op == '/') values.push(b == 0 ? 0 : a / b);
    };

    auto prec = [](char c) {
        if (c == '+' || c == '-') return 1;
        if (c == '*' || c == '/') return 2;
        return 0;
    };

    char c;
    while (in >> c) {
        if (isdigit(c) || c == '.') {
            in.putback(c);
            double val;
            in >> val;
            values.push(val);
        } else if (c == '(') {
            ops.push(c);
        } else if (c == ')') {
            while (!ops.empty() && ops.top() != '(') {
                apply(ops.top());
                ops.pop();
            }
            if (!ops.empty()) ops.pop();
        } else if (strchr("+-*/", c)) {
            while (!ops.empty() && prec(ops.top()) >= prec(c)) {
                apply(ops.top());
                ops.pop();
            }
            ops.push(c);
        }
    }

    while (!ops.empty()) {
        apply(ops.top());
        ops.pop();
    }

    return values.empty() ? 0 : values.top();
}

// --- Main GUI app ---
int main() {
    App app("EasyCPP Calculator", 320, 420);

    Label display(app, "0", 10, 10, 300, 40);
    std::string expr;

    const char* buttons[4][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", "*"},
        {"1", "2", "3", "-"},
        {"0", "C", "=", "+"}
    };

    int startY = 60;
    int bw = 70, bh = 60, gap = 10;

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            std::string t = buttons[r][c];
            Button(app, t, 10 + c * (bw + gap), startY + r * (bh + gap), bw, bh, [&, t]() {
                if (t == "C") {
                    expr.clear();
                    display.set("0");
                } else if (t == "=") {
                    try {
                        double res = eval_expr(expr);
                        std::ostringstream ss;
                        ss << res;
                        expr = ss.str();
                        display.set(expr);
                    } catch (...) {
                        display.set("Error");
                        expr.clear();
                    }
                } else {
                    expr += t;
                    display.set(expr);
                }
            });
        }
    }

    app.run();
    return 0;
}
