#include <fstream>
#include "MIPSgen.h"
using namespace std;

int TOP_OF_STACK = -8;
int WHILE_COUNT = 0;
int IF_COUNT = 0;
int DELETE_COUNT = 0;

void immediate(string num) {
    cout << "lis $3" << endl;
    cout << ".word " << num << endl;
}

void code(string id, map<string,pair<string,int>> symbolTable) {
    cout << "lw $3, " << to_string(symbolTable[id].second) << "($29)" << endl; 
}

void push(string reg) {
    cout << "sw " << reg << ", -4($30)" << endl;
    cout << "sub $30, $30, $4" << endl;
}

void pop(string reg) {
    cout << "add $30, $30, $4" << endl;
    cout << "lw " << reg << ", -4($30)" << endl;
}

string checkWellTyped(string left, string right, string OP, string &mips = placeholder) {
    if (left == EMPTY_STRING) {
        if (OP == AMP) {
	    if (right != INT) {
                throw typeError("ERROR : cannot take address of pointer");
	    } else {
	        return POINTER;
	    }
        } else if (OP == STAR) {
	    if (right != POINTER) {
                throw typeError("ERROR : cannot dereference a non-pointer");
	    } else {
	        return INT;
	    }
	}	    
    } else {
        if (OP == PLUS) {
            mips = "add ";
            if (left == POINTER && right == POINTER) {
                throw typeError("ERROR : cant add two pointers");
            } else if (left == INT && right == INT) {
	        return INT;	
	    } else {  
	        return POINTER;
	    }
        } else if (OP == MINUS) {
            mips = "sub ";
            if (left == INT && right == POINTER) {
                throw typeError("ERROR : can't subtract pointer from int");
            } else if (left == POINTER && right == INT) {
	        return POINTER;
	    } else {
	        return INT;
	    }
        } else if (OP == STAR || OP == SLASH || OP == PCT) {
            if (left != INT || right != INT) {
                throw typeError("ERROR : attempting to use integer operation on pointer");
            } else {
	        return INT;
	    }
        }
    }
  
    return EMPTY_STRING;
}

string checkType(Node * root, map<string,pair<string,int>> &symbolTable, string &mips) {

    if (head(root->symbol) == ID) {

        string id = tail(root->symbol);
        auto found = symbolTable.find(id);
	
        if (found == symbolTable.end()) {
            id = "ERROR : ID " + id + " not declared.";
            throw typeError(id);
        } else {
            mips = id;
            code(id, symbolTable);
            return symbolTable[id].first;
        }
    } else if (head(root->symbol) == NUM) {
        immediate(tail(root->symbol));
        return INT;

    } else if (head(root->symbol) == NIL) {
        cout << "add $3, $0, $11" << endl;
        return POINTER;
    } else if (head(root->symbol) == LVAL) {

        if (root->children.size() == 1) {
		cout << "; lvalue -> ID" << endl;
		return checkType(root->children[0],symbolTable, mips);
	}
        if (root->children.size() == 3) return checkType(root->children[1],symbolTable, mips);

        string OP = head(root->children[0]->symbol);
        string left = EMPTY_STRING;
        string right = checkType(root->children[1],symbolTable, mips);

        return checkWellTyped(left,right,OP);
        
    } else if (head(root->symbol) == EXPR) {

        if (root->children.size() == 1) return checkType(root->children[0],symbolTable, mips);

        string OP = head(root->children[1]->symbol);

        string left = checkType(root->children[0],symbolTable, mips);
	string LHS = mips;
        push("$3");

        string right = checkType(root->children[2],symbolTable, mips);
   
        string ret_type = checkWellTyped(left,right,OP,mips);
        
	if (left == INT && right == INT) {			// int+int OR int-int
	    pop("$5");
            cout << mips << " $3, $5, $3" << endl;        
	} else if (left == POINTER && right == POINTER) {	// int * - int *
	    pop("$5");
	    cout << "sub $3, $5, $3" << endl;
	    cout << "div $3, $4" << endl;
	    cout << "mflo $3" << endl;
	} else {
	    if (right == INT) {				// int* + int OR int* - int
	        cout << "mult $3, $4" << endl;
		cout << "mflo $3" << endl;
		pop("$5");
	    } else {		    			// int + int*
		// $3 = term, TOS = expr
	        pop("$5");
		cout << "mult $5, $4" << endl;
		cout << "mflo $5" << endl;
	    }
	    cout << mips << " $3, $5, $3" << endl;
	    mips = LHS;
	    if (OP == MINUS) mips = EMPTY_STRING;
	}
        return ret_type;

    } else if (head(root->symbol) == TERM) {
        
        if (root->children.size() == 1) return checkType(root->children[0],symbolTable, mips);

        string OP = head(root->children[1]->symbol);
        string left = checkType(root->children[0],symbolTable, mips);
        push("$3");
        string right = checkType(root->children[2],symbolTable, mips);
        pop("$5");

        string ret_type = checkWellTyped(left,right,OP,mips);

        if (OP == STAR) {
            cout << "mult $3, $5" << endl;
            cout << "mflo $3" << endl;
        } else {
            cout << "div $5, $3" << endl;
            if (OP == PCT) {
                cout << "mfhi $3" << endl;
            } else {
                cout << "mflo $3" << endl;
            }
        }

        return ret_type;
    } else if (head(root->symbol) == FACTOR) {
        if (root->symbol == PROCEDURE_NOPARAMS || root->symbol == PROCEDURE_PARAMS) {
	    vector<string> signature;
	    string id = tail(root->children[0]->symbol);
	        
            checkProcedure(id,symbolTable);
	    buildArgList(root->children[2], signature,false,symbolTable);
	        
            if (signature != TABLE[id].first) {
	        id = "ERROR : Signature mismatch for procedure " + id;
	        throw typeError(id);
	    } else {
	        return INT;
	    }
	}

        if (root->children.size() == 1) return checkType(root->children[0],symbolTable, mips);
        if (root->children.size() == 3) return checkType(root->children[1],symbolTable, mips);

        if (root->children.size() == 5) {
            string newptr = checkType(root->children[3],symbolTable, mips);	// code(expr)
            if (newptr != INT) {
                throw typeError("ERROR : invalid pointer");
            } else {
		cout << "add $1, $3, $0" << endl;
		push("$31");
		cout << "lis $5\n.word new" << endl;
		cout << "jalr $5" << endl;
		pop("$31");
		cout << "bne $3, $0, 1" << endl;
		cout << "add $3, $11, $0" << endl;
	        return POINTER;
	    }
        }

        string OP =  head(root->children[0]->symbol);
        string left = EMPTY_STRING;
        string right = checkType(root->children[1],symbolTable, mips);
        
        if (OP == STAR) cout << "lw $3, 0($3)" << endl;
        if (OP == AMP) {
            string offset = mips;       // factor -> AMP lvalue 
	    if (mips != EMPTY_STRING) {
		string offset = to_string(symbolTable[mips].second);
		immediate(offset);
		
		cout << "add $3, $3, $29" << endl;
	    }
        }
	left = EMPTY_STRING;
	
        return checkWellTyped(left,right,OP);

    } else if (head(root->symbol) == TEST) {
	
        string OP = head(root->children[1]->symbol);
	string left = checkType(root->children[0],symbolTable, mips);		// code(expr1)
	push("$3");
        string right = checkType(root->children[2],symbolTable, mips);		// code(expr2)
	pop("$5");

        if (left != right) {
            throw typeError("ERROR : cannot perform comparison on different types");
        }
    	string comp = "slt ";       
    	if (left == POINTER) comp = "sltu ";		// for pointers

	if (OP == LT || OP == GE) {
	    cout << comp << "$3, $5, $3" << endl;
	    if (OP == GE) cout << "sub $3, $11, $3" << endl;
	} else if (OP == GT || OP == LE) {
	    cout << comp << "$3, $3, $5" << endl;
	    if (OP == LE) cout << "sub $3, $11, $3" << endl;
	} else if (OP == NE || OP == EQ) {
	    cout << comp << "$6, $3, $5" << endl;
	    cout << comp << "$7, $5, $3" << endl;
	    cout << "add $3, $6, $7" << endl;
	    if (OP == EQ) cout << "sub $3, $11, $3" << endl;
	}
    } else if (root->symbol == IF) {
	    
	    string else_label = "else" + to_string(IF_COUNT);
	    string endif_label = "endif" + to_string(IF_COUNT);
	    IF_COUNT++;
	    
            checkType(root->children[2],symbolTable, mips);			// code(test)
	    cout << "beq $3, $0, " << else_label << endl;			// beq $3, $0, else
	     
	    checkType(root->children[5],symbolTable, mips);			// code(stmts1)
	    cout << "beq $0, $0, " << endif_label << endl;			// beq $0, $0, endif
	    
	    cout << else_label << ":" << endl;					// else:
	    checkType(root->children[9],symbolTable, mips);			// code(stmts2)
    	    
	    cout << endif_label << ":" << endl;					// endif:
	   
    } else if (root->symbol == WHILE) {

	    string loop = "loop" + to_string(WHILE_COUNT);
	    string end_while = "endWhile" + to_string(WHILE_COUNT);
	    WHILE_COUNT++;

	    cout << loop << ":" << endl;				        // loop:
	    checkType(root->children[2],symbolTable, mips);			// code(test)
	    cout << "beq $3, $0, "<< end_while << endl;	
	    checkType(root->children[5],symbolTable, mips);			// code(statements)
	    cout << "beq $0, $0, " << loop << endl;	
	    cout << end_while << ":" << endl;		                        // endWhile:
	  
    } else if (root->symbol == PRINTLN) {
        
	push("$1");		
    	string expr = checkType(root->children[2],symbolTable, mips);		// code(expr)
	if (expr != INT) {
	    throw typeError("ERROR : cannot print pointer");
	}
        cout << "add $1, $3, $0" << endl;
        push("$31");
        cout << "jalr $10" << endl;
        pop("$31");
        pop("$1");

    } else if (root->symbol == DELETE) {
        string expr = checkType(root->children[3],symbolTable, mips);	// code(expr)
	string label = "skipDelete" + DELETE_COUNT;
	DELETE_COUNT++;

	if (expr != POINTER) {
	    throw typeError("ERROR : cannot deallocate an int");
	}
	cout << "beq $3, $11, " << label << endl;
	cout << "add $1, $3, $0" << endl;
	push("$31");
	cout << "lis $5\n.word delete" << endl;
	cout << "jalr $5" << endl;
	pop("$31");
	cout << label << ":" << endl;
    } else if (root->symbol == ASSIGNMENT) {
	
        string left = checkType(root->children[0],symbolTable, mips);
        string LHS = mips;
        string right = checkType(root->children[2],symbolTable, mips);
        // value of RHS comes back as $3

        if (left != right) {
            throw typeError("ERROR : assignment of different types");
        }

	// assignment through pointer dereference
	if (left == INT && symbolTable[LHS].first == POINTER) {
	    // code (expr) 
	    cout << "; assignment throug pointer dereference" << endl;
	    push("$3");
	    checkType(root->children[0],symbolTable, mips);		// code(factor)
	    pop("$5");
	    cout << "sw $5, 0($3)" << endl;
	} else {
            int offset = symbolTable[LHS].second;
	    
            cout << "sw $3, " << to_string(offset) << "($29)" << endl;
	}
    } else if (head(root->symbol)== STATEMENTS && root->children.size() == 2) {
        checkType(root->children[0],symbolTable,mips);
    	return checkType(root->children[1],symbolTable, mips);
    } else {
        for (auto child : root->children) {
            checkType(child,symbolTable, mips);
        }
    }
    return EMPTY_STRING;
}

void checkProcedure(string id, map<string,pair<string,int>> symbolTable) {
    auto found = TABLE.find(id);
    if (found == TABLE.end()) {
        id = "ERROR : Procedure " + id + " not declared.";
        throw typeError(id);
    }
}

void preorderTraverse(Node * root, vector<string> &ret, map<string,pair<string,int>> &symbolTable, bool init = false) {


    string placeholder = ""; 
    if (head(root->symbol) == DCL) {	// params
        getType(root,ret);
        
        addNewSymbol(ret,symbolTable);
        ret.clear();
    } else if (head(root->symbol) == DCLS) {		// decls
        if (root->children.size() != 0) {

            preorderTraverse(root->children[0], ret, symbolTable);
            getType(root->children[1],ret);

            string type = INT;
            if (ret.size() == 3) type = POINTER;

            string init = head(root->children[3]->symbol);

            if (type == INT) {
                if (init != NUM) throw typeError("ERROR : INT must be initialized to an integer");
		init = tail(root->children[3]->symbol);

		immediate(init);
            } else if (type == POINTER) {
                if (init != NIL) throw typeError("ERROR : INT* must be initialized to NULL");
		cout << "add $3, $0, $11" << endl;
            } 

	    addNewSymbol(ret,symbolTable);
            cout << "sw $3, " << TOP_OF_STACK << "($29)         ; push variable " << ret.back() << endl;
            TOP_OF_STACK -= 4;
            cout << "sub $30, $30, $4       ; update stack pointer" << endl;

            ret.clear();
        }
    } else if (head(root->symbol) == STATEMENTS) {
	    checkType(root,symbolTable);
    } else if (head(root->symbol) == EXPR) {	          // return
        string ret_type = checkType(root,symbolTable);
        if (ret_type != INT) {
            throw typeError("ERROR : procedure must return INT");
        }
    } else if (root->children.size() != 0) {
        for (auto child : root->children) {
	    preorderTraverse(child,ret,symbolTable);
	}
    } 
}

void buildArgList(Node * root, vector<string> &signature,bool init, map<string,pair<string,int>> symbolTable) {
    vector<string> ret;
    if (!init && head(root->symbol) == ARGLIST) {
        
	string type = checkType(root->children[0],symbolTable);
	signature.push_back(type);
    	
	if (root->children.size() == 3) {
	    buildArgList(root->children[2],signature,false,symbolTable);
	}
    } else {
        if (head(root->symbol) == DCL) {
            getType(root, ret);
            if (ret.size() == 3) {
                signature.push_back(POINTER);
            } else {
	        signature.push_back(INT);
            }
        } else if (root->children.size() != 0) {
            for (auto child : root->children) {
                buildArgList(child, signature);
            }
        }
    }
}

void init() {
    cout << "\n; first run of init" << endl;
    push("$31");
    cout << "lis $5" << endl;
    cout << ".word init" << endl;
    cout << "jalr $5" << endl;
    pop("$31");
    cout << endl;
}

void buildSymbolTable(Node * root, vector<string> &ret, map<string,pair<string,int>> &symbolTable) {
    vector<string> signature;

    if (tail(root->symbol) == MAIN) {
        
        string id = WAIN;

        buildArgList(root->children[0]->children[3],signature);
        buildArgList(root->children[0]->children[5],signature);

        if (signature[1] != INT) {
            throw typeError("ERROR : second parameter of WAIN must be int");
        }

	if (signature[0] != POINTER) {
	    push("$2");
	    cout << "add $2, $0, $0" << endl;
	    init();
	    pop("$2");
	} else {
	    init();
	}

        preorderTraverse(root->children[0],ret,symbolTable);
        addNewProcedure(id,signature,symbolTable);

    } else if (head(root->symbol) == PROCEDURE && tail(root->symbol) != PROCEDURE) {

        string id = tail(root->children[1]->symbol);
        buildArgList(root->children[3],signature);
	addNewProcedure(id,signature,symbolTable);

        preorderTraverse(root,ret,symbolTable);

        addNewProcedure(id,signature,symbolTable,false);

    } else if (root->children.size() != 0) {
        for (auto child : root->children) {
            buildSymbolTable(child,ret,symbolTable);
        }
    }
}

int main() {
    
    ifstream CFG;
    CFG.open("states.txt");
  
    int numNonTerminals;
    CFG >> numNonTerminals;
    
    NON_TERMINALS.resize(numNonTerminals);

    for (int i = 0; i < numNonTerminals; i++) {
        CFG >> NON_TERMINALS[i];
    }
    
    vector<string> ret;
    map<string,pair<string,int>> symbolTable;
    
    Node * root = buildTree(); 
    prologue();
    try {
        buildSymbolTable(root,ret,symbolTable);
    } catch (typeError & e) {
        cerr << e.print() << endl;
        deleteTree(root);
	epilogue(WAIN);
        return 1;
    }
    
    epilogue(WAIN);
    deleteTree(root);

    return 0;
}




