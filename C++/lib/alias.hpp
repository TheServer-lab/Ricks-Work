#ifndef ALIAS_HPP // Prevent multiple inclusions of this file
#define ALIAS_HPP

#include <bits/stdc++.h> // GCC-specific header that includes all standard libraries
using namespace std;     // Avoids needing "std::" before standard library names

// ================== TYPE ALIASES ==================
// Shorter names for common types
using ll  = long long;              // 64-bit integer
using ull = unsigned long long;     // Unsigned 64-bit integer
using ld  = long double;            // Extended precision floating-point
using str = string;                 // String type
using pii = pair<int,int>;          // Pair of ints
using pll = pair<ll,ll>;            // Pair of long longs
using vi  = vector<int>;            // Vector of ints
using vll = vector<ll>;             // Vector of long longs
using vs  = vector<string>;         // Vector of strings

// ================== MACROS ==================
// Handy shortcuts for common operations
#define all(x) (x).begin(), (x).end()          // Range from beginning to end
#define rall(x) (x).rbegin(), (x).rend()       // Reverse range (end to beginning)
#define fastio ios::sync_with_stdio(false); cin.tie(nullptr) // Faster I/O
#define rep(i,a,b) for (int i = (a); i < (b); ++i) // For loop shorthand
#define each(x,a) for (auto &x : a)                 // Range-based loop shorthand
#define yes cout << "YES\n"                         // Output YES
#define no  cout << "NO\n"                          // Output NO

// ================== CONSTANTS ==================
const ll    INF = 1e18;                       // Large "infinity" value
const int   MOD = 1e9 + 7;                     // Common modulus for modular arithmetic
const double EPS = 1e-9;                       // Small epsilon for floating-point comparisons
const double PI  = 3.14159265358979323846;     // Pi constant

// ================== I/O HELPERS ==================
// Variadic template print functions for convenience
template<typename... Args> 
void print(Args&&... args) { (cout << ... << args); }   // Print without newline

template<typename... Args> 
void println(Args&&... args) { (cout << ... << args) << "\n"; } // Print with newline

// ================== DEBUGGING ==================
// Debug print to cerr so it doesn't interfere with program output
template<typename T> 
void debug(const T &x) { cerr << "[DEBUG] " << x << "\n"; }

template<typename A, typename B> 
void debug(const pair<A,B> &p) { cerr << "(" << p.first << ", " << p.second << ")\n"; }

template<typename T> 
void debug(const vector<T> &v) { cerr << "[ "; for (auto &i : v) cerr << i << " "; cerr << "]\n"; }

// ================== MATH HELPERS ==================
ll gcdll(ll a, ll b) { return b ? gcdll(b, a % b) : a; }        // Greatest common divisor
ll lcmll(ll a, ll b) { return a / gcdll(a,b) * b; }             // Least common multiple

bool isPrime(ll n) { // Simple prime check
    if (n < 2) return false;
    for (ll i = 2; i * i <= n; ++i) 
        if (n % i == 0) return false;
    return true;
}

ll factorial(ll n) { return (n <= 1 ? 1 : n * factorial(n - 1)); } // Recursive factorial

ll nCr(ll n, ll r) { // Combinations: n choose r
    if (r < 0 || r > n) return 0;
    r = min(r, n - r);
    ll num = 1, den = 1;
    for (ll i = 0; i < r; ++i) {
        num *= (n - i);
        den *= (i + 1);
    }
    return num / den;
}

ll modpow(ll a, ll e, ll m = MOD) { // Modular exponentiation
    ll r = 1;
    while (e) {
        if (e & 1) r = (r * a) % m;
        a = (a * a) % m;
        e >>= 1;
    }
    return r;
}

// ================== RANDOM HELPERS ==================
// Random integer in [l, r]
int randInt(int l, int r) {
    static mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    return uniform_int_distribution<int>(l, r)(rng);
}

// Random double in [l, r]
double randDouble(double l, double r) {
    static mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    return uniform_real_distribution<double>(l, r)(rng);
}

// Shuffle vector randomly
template<typename T> 
void shuffleVec(vector<T> &v) {
    static mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    shuffle(all(v), rng);
}

// ================== STRING HELPERS ==================
// Remove whitespace from both ends
str trim(const str &s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    return (start == string::npos) ? "" : s.substr(start, end - start + 1);
}

// Split string by delimiter
vs split(const str &s, char delim) {
    vs res; stringstream ss(s); str item;
    while (getline(ss, item, delim)) res.push_back(item);
    return res;
}

// Join vector of strings with separator
str join(const vs &v, const str &sep) {
    stringstream ss;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) ss << sep;
        ss << v[i];
    }
    return ss.str();
}

// Convert string to lowercase
str toLower(str s) { transform(all(s), s.begin(), ::tolower); return s; }

// Convert string to uppercase
str toUpper(str s) { transform(all(s), s.begin(), ::toupper); return s; }

// ================== FILE HELPERS ==================
// Read file into string
str readFile(const str &filename) {
    ifstream in(filename);
    if (!in) return "";
    stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Write string to file
bool writeFile(const str &filename, const str &content) {
    ofstream out(filename);
    if (!out) return false;
    out << content;
    return true;
}

#ifdef ALIAS_MAIN // Optional menu-based program section

// -------- Math menu --------
void menu_math() {
    println("--- Math Tools ---");
    println("1) GCD");
    println("2) LCM");
    println("3) Prime Check");
    println("4) Factorial");
    println("5) nCr");
    int choice; cin >> choice;
    ll a, b;
    switch(choice) {
        case 1: cin >> a >> b; println("GCD: ", gcdll(a,b)); break;
        case 2: cin >> a >> b; println("LCM: ", lcmll(a,b)); break;
        case 3: cin >> a; println(isPrime(a) ? "Prime" : "Not Prime"); break;
        case 4: cin >> a; println("Factorial: ", factorial(a)); break;
        case 5: cin >> a >> b; println("nCr: ", nCr(a,b)); break;
        default: println("Invalid option");
    }
}

// -------- String menu --------
void menu_string() {
    println("--- String Tools ---");
    println("1) Trim");
    println("2) Split");
    println("3) Join");
    println("4) ToLower");
    println("5) ToUpper");
    int choice; cin >> choice;
    string s; getline(cin, s); // consume newline
    switch(choice) {
        case 1: getline(cin, s); println("Trimmed: '", trim(s), "'"); break;
        case 2: getline(cin, s); {
            char delim; cin >> delim;
            auto parts = split(s, delim);
            println("Split into ", parts.size(), " parts");
            each(p, parts) println(p);
        } break;
        case 3: {
            int n; cin >> n;
            vs arr(n); for(int i=0;i<n;i++) cin >> arr[i];
            string sep; cin >> sep;
            println("Joined: ", join(arr, sep));
        } break;
        case 4: getline(cin, s); println(toLower(s)); break;
        case 5: getline(cin, s); println(toUpper(s)); break;
        default: println("Invalid option");
    }
}

// -------- Random menu --------
void menu_random() {
    println("--- Random Tools ---");
    println("1) Random int");
    println("2) Random double");
    println("3) Shuffle vector<int>");
    int choice; cin >> choice;
    int a, b;
    switch(choice) {
        case 1: cin >> a >> b; println(randInt(a,b)); break;
        case 2: {
            double x, y; cin >> x >> y; println(randDouble(x,y));
        } break;
        case 3: {
            int n; cin >> n; vi v(n); rep(i,0,n) cin >> v[i];
            shuffleVec(v);
            each(x,v) print(x," "); println();
        } break;
        default: println("Invalid option");
    }
}

// -------- File menu --------
void menu_file() {
    println("--- File Tools ---");
    println("1) Write File");
    println("2) Read File");
    int choice; cin >> choice;
    string filename, content;
    getline(cin, filename); // consume newline
    switch(choice) {
        case 1: getline(cin, filename); getline(cin, content);
                println(writeFile(filename, content) ? "OK" : "FAIL");
                break;
        case 2: getline(cin, filename);
                println(readFile(filename));
                break;
        default: println("Invalid option");
    }
}

// -------- Main menu loop --------
int main() {
    fastio;
    while (true) {
        println("\n=== ALIAS TOOLKIT ===");
        println("1) Math Tools");
        println("2) String Tools");
        println("3) Random Tools");
        println("4) File Tools");
        println("0) Exit");
        int choice; cin >> choice;
        if (choice == 0) break;
        if (choice == 1) menu_math();
        else if (choice == 2) menu_string();
        else if (choice == 3) menu_random();
        else if (choice == 4) menu_file();
        else println("Invalid option");
    }
    println("Bye!");
    return 0;
}

#endif // ALIAS_MAIN
#endif // ALIAS_HPP
