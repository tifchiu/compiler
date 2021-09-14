#ifndef _MIPSGEN_H_
#define _MIPSGEN_H_

#include <vector>
#include <string>
#include <map>
#include <utility>
#include <iostream>
#include <sstream>
#include <algorithm>


using namespace std;

////////////////////////// CONSTANTS, GLOBALS, AND FUNCTION DECLARATIONS FOR MIPSGEN.CC //////////////////////////////////

map<string, pair<vector<string>, map<string,pair<string,int>>>> TABLE;
vector<string> NON_TERMINALS;
map<string,pair<string,int>> EMPTY;

const string EMPTY_STRING = "";
static string placeholder = "";

const string INT = "int";
const string POINTER = "int*";
const string PROCEDURE_NOPARAMS = "factor ID LPAREN RPAREN";
const string PROCEDURE_PARAMS = "factor ID LPAREN arglist RPAREN";
const string ASSIGNMENT = "statement lvalue BECOMES expr SEMI";
const string IF = "statement IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE";
const string WHILE = "statement WHILE LPAREN test RPAREN LBRACE statements RBRACE";
const string PRINTLN = "statement PRINTLN LPAREN expr RPAREN SEMI";
const string DELETE = "statement DELETE LBRACK RBRACK expr SEMI";
const string PROCEDURE = "procedure";
const string STATEMENTS = "statements";
const string EXPR = "expr";
const string MAIN = "main";
const string WAIN = "wain";
const string ARGLIST = "arglist";
const string NIL = "NULL";
const string NUM = "NUM";
const string TERM = "term";
const string FACTOR = "factor";
const string LVAL = "lvalue";
const string ID = "ID";
const string AMP = "AMP";
const string STAR = "STAR";
const string PLUS = "PLUS";
const string MINUS = "MINUS";
const string SLASH = "SLASH";
const string PCT = "PCT";
const string TEST = "test";
const string DCL = "dcl";
const string DCLS = "dcls";
const string LT = "LT";
const string EQ = "EQ";
const string NE = "NE";
const string LE = "LE";
const string GE = "GE";
const string GT = "GT";
const string ADD = "add";
const string SUB = "sub";

struct Node {
    string symbol;
    vector<Node*> children;
};

class typeError {
    private:
    string message;
    
    public:
    typeError(string msg) : message{msg} {}
    string print() { return message; }

};

// forward declarations of functions 
void buildArgList(Node * root, vector<string> &signature, bool init = true, map<string,pair<string,int>> symbolTable = EMPTY);
void checkProcedure(string id, map<string,pair<string,int>> symbolTable);
string checkType(Node * root, map<string,pair<string,int>> &symbolTable, string &mips=placeholder);

void print(vector<string> v) {
    for (auto i : v) {
        cerr << i << " ";
    }
    cerr << endl;
}

void printTable() {
    for (auto i : TABLE) {
        cerr << i.first << ": ";
        print(i.second.first);
        for (auto symbol : i.second.second) {
            cerr << symbol.first << " " << symbol.second.first << " " << symbol.second.second << endl;
        }
    }
}

bool isNonTerminal(string s) {
    auto it = find(NON_TERMINALS.begin(), NON_TERMINALS.end(), s);
    if (it != NON_TERMINALS.end()) {
        return true;
    } else {
        return false;
    }
}

string head(string line) {
    string lookahead;
    stringstream sline(line);
    sline >> lookahead;
    return lookahead;
}

string tail(string line) {
    string lookahead;
    stringstream sline(line);
    while (sline >> lookahead) {
        continue;
    }
    return lookahead;
}

void getType(Node * root, vector<string> &ret) {
    if (root->children.size() == 0) {
        ret.push_back(tail(root->symbol));
    } else {
        for (auto child : root->children) {
            getType(child,ret);
        }
    }
}

void addNewSymbol(vector<string> &ret, map<string,pair<string,int>> &symbolTable) {
    string key, value;
    if (ret.size() == 3) {
        value = POINTER;
        key = ret.back();
    } else {
        value = INT;
        key = ret.back();
    }

    auto it = symbolTable.find(key);
    if (it == symbolTable.end()) {      // not found
        int offset = symbolTable.size() * -4; 
        pair<string,int> right = make_pair(value,offset);
        symbolTable.insert(pair<string,pair<string,int>>(key,right));
    } else {
	    key = "ERROR : ID " + key + " cannot be reassigned.";
        throw typeError(key);
    }
}

void addNewProcedure(string id, vector<string> &signature, map<string,pair<string,int>> &symbolTable, bool init = true) {
    auto found = TABLE.find(id);
    if (found != TABLE.end() && !init) {
        TABLE[id].second = symbolTable;
    } else if (found == TABLE.end()) {
    	TABLE.insert(pair<string,pair<vector<string>, map<string,pair<string,int>>>>(id, pair<vector<string>, map<string,pair<string,int>>>(signature,symbolTable)));
    } else {
        id = "ERROR : Procedure " + id + " already declared.";
        throw typeError(id);
    }

    signature.clear();
    symbolTable.clear();
}


void deleteTree(Node * root) {
    if (root->children.size() == 0) {
        delete root;
    } else {
        for (auto child : root->children) {
            deleteTree(child); 
        }
        delete root;
    }
}

Node * buildTree() {
    string line, temp;
    getline(cin,line);
    vector<string> symbol;
    
    stringstream sline(line);
    while (sline >> temp) {
        symbol.push_back(temp);
    }

    Node * root = new Node{line};

    if (isNonTerminal(symbol[0])) {
        int numChildren = symbol.size()-1;
        root->children.resize(numChildren);
        for (int i = 0; i < numChildren; i++) {
            root->children[i] = buildTree();
        }
    } 
    return root;
}

void prologue() {
    cout << "; begin prologue" << endl;
    cout << ".import init\n.import new\n.import delete" << endl;
    cout << "lis $4\n.word 4" << endl;
    cout << ".import print\nlis $10\n.word print" << endl;
    cout << "lis $11\n.word 1" << endl;
    cout << "sub $29, $30, $4       ; setup frame pointer" << endl;
    cout << "sw $1, 0($29)          ; push variable a" << endl;
    cout << "sub $30, $30, $4       ; update stack pointer" << endl;
    cout << "sw $2, -4($29)         ; push variable b" << endl;
    cout << "sub $30, $30, $4       ; update stack pointer" << endl;
    cout << "; end prologue\n" << endl;
}


void epilogue(string procedure) {
    cout << "\n; begin epilogue" << endl;
    int num_vars = TABLE[procedure].second.size();
    for (int i = 0; i < num_vars; i++) {
        cout << "add $30, $30, $4" << endl;
    }
    cout << "jr $31" << endl;
}


#endif




