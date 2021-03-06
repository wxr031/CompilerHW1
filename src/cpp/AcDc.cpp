#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

enum TokenType {
  DECL_INT,    // integer declaration ("i")
  DECL_FLOAT,  // float declaration ("f")
  CMD_PRINT,   // print command ("p")
  CONST_INT,   // integer constant
  CONST_FLOAT, // float constant
  IDENTIFIER,
  BIN_OP_ADD,    // addition operator ("+")
  BIN_OP_SUB,    // subtraction operator ("-")
  BIN_OP_MUL,    // subtraction operator ("*")
  BIN_OP_DIV,    // subtraction operator ("/")
  BIN_OP_ASSIGN, // assignment operator ("=")
  LEFT_PARAM,    // left parentheses
  RIGHT_PARAM,   // right parentheses
  END_OF_FILE    // EOF
};

struct Token {
  TokenType Type;
  std::string Value;
};

template <typename... ArgsT> void emitErrorImpl(ArgsT &&... Args);
template <> void emitErrorImpl<>() {}

template <typename HeadT, typename... TailT>
void emitErrorImpl(HeadT &&H, TailT &&... T) {
  std::cerr << H;
  emitErrorImpl(std::forward<TailT>(T)...);
}

/// Helper function of emitting the error to standard error.
template <typename... ArgsT>
[[noreturn]] void emitError(std::string E, ArgsT &&... Args) {
  std::cerr << "[Error] " << E << ": ";
  emitErrorImpl(std::forward<ArgsT>(Args)...);
  std::cerr << "\n";
  exit(1);
}

class Tokenizer {
  std::ifstream IFS;
  Token CurToken;
  size_t NumWhiteSpaces;

  Token getNextToken();
  Token getNumericToken(char C);
  Token getDeclOrIDToken(char C);
  Token getOpToken(char C) const;

public:
  Tokenizer(std::ifstream &&S)
      : IFS(std::move(S)), CurToken(getNextToken()), NumWhiteSpaces(0) {}

  operator bool() const { return CurToken.Type != END_OF_FILE; }
  bool hasReadWhiteSpaces() const { return NumWhiteSpaces != 0; }
  Token readToken();
  const Token &peekToken();
};

Token Tokenizer::getNumericToken(char C) {
  Token Tok;
  while (isdigit(C)) {
    Tok.Value += C;
    C = IFS.get();
  }
  if (C != '.') {
    Tok.Type = CONST_INT;
  } else {
    Tok.Type = CONST_FLOAT;
    Tok.Value += '.';
    while (C = IFS.get(), isdigit(C))
      Tok.Value += C;
  }
  IFS.unget();
  return Tok;
}

Token Tokenizer::getDeclOrIDToken(char C) {
  std::string Value = "";
  while (isalpha(C)) {
    Value += C;
    C = IFS.get();
  }
  IFS.unget();
  Token Tok;
  if (Value == "i") {
    Tok.Type = DECL_INT;
  }
  else if (Value == "f") {
    Tok.Type = DECL_FLOAT;
  }
  else if (Value == "p") {
    Tok.Type = CMD_PRINT;
  }
  else {
    Tok.Type = IDENTIFIER;
    Tok.Value = Value;
  }
  return Tok;
}

Token Tokenizer::getOpToken(char C) const {
  switch (C) {
  case '+':
    return Token{BIN_OP_ADD};
  case '-':
    return Token{BIN_OP_SUB};
  case '*':
    return Token{BIN_OP_MUL};
  case '/':
    return Token{BIN_OP_DIV};
  case '=':
    return Token{BIN_OP_ASSIGN};
  case '(':
    return Token{LEFT_PARAM};
  case ')':
    return Token{RIGHT_PARAM};
  default:
    emitError("Tokenizer", "expecting '+', '-', or '=', but '", C, "' found.");
  }
}

Token Tokenizer::getNextToken() {
  char C = IFS.get();
  NumWhiteSpaces = 0;
  while (isspace(C)) {
    C = IFS.get();
    NumWhiteSpaces++;
  }

  if (C == EOF)
    return Token{END_OF_FILE};
  if (isdigit(C))
    return getNumericToken(C);
  if (isalpha(C))
    return getDeclOrIDToken(C);

  return getOpToken(C);
}

const Token &Tokenizer::peekToken() { return CurToken; }

std::ostream &operator<<(std::ostream &S, TokenType T) {
  switch (T) {
  case DECL_INT:
    return S << "DECL_INT";
  case DECL_FLOAT:
    return S << "DECL_FLOAT";
  case CMD_PRINT:
    return S << "CMD_PRINT";
  case CONST_INT:
    return S << "CONST_INT";
  case CONST_FLOAT:
    return S << "CONST_FLOAT";
  case IDENTIFIER:
    return S << "IDENTIFIER";
  case BIN_OP_ADD:
    return S << "BIN_OP_ADD";
  case BIN_OP_SUB:
    return S << "BIN_OP_SUB";
  case BIN_OP_MUL:
    return S << "BIN_OP_MUL";
  case BIN_OP_DIV:
    return S << "BIN_OP_DIV";
  case LEFT_PARAM:
    return S << "LEFT_PARAM";
  case RIGHT_PARAM:
    return S << "RIGHT_PARAM";
  case BIN_OP_ASSIGN:
    return S << "BIN_OP_ASSIGN";
  case END_OF_FILE:
    return S << "END_OF_FILE";
  }
  __builtin_unreachable();
}

Token Tokenizer::readToken() {
  Token Result(std::move(CurToken));
  CurToken = getNextToken();
  return Result;
}

enum VariableType { VAR_INT, VAR_FLOAT };

enum ASTNodeType {
  PROGRAM_NODE,
  DECLARATION_NODE,
  ASSIGNMENT_NODE,
  PRINTSTMT_NODE,
  IDENTIFIER_NODE,
  CONST_INT_NODE,
  CONST_FLOAT_NODE,
  BIN_ADD_NODE,
  BIN_SUB_NODE,
  BIN_MUL_NODE,
  BIN_DIV_NODE,
  CONVERSION_NODE
};

enum DataType { DATA_INT, DATA_FLOAT };

struct AST {
  ASTNodeType Type;
  std::vector<AST *> SubTree;
  std::variant<int, float, size_t, DataType> Value;

  AST() = default;
  AST(ASTNodeType T) : Type(T) {}
  AST(ASTNodeType T, AST *Child) : Type(T), SubTree({Child}) {}

  ~AST() {
    for (AST *Node : SubTree)
      delete Node;
    SubTree.clear();
  }
};

class SymbolTable {
  std::unordered_map<std::string, size_t> VarID;
  std::vector<VariableType> VarType;

public:
  size_t declareSymbol(std::string ID, TokenType DeclType) {
    if (VarID.find(ID) != VarID.end())
      emitError("SymbolTable", "redeclaration of variable ", ID);
    size_t Res = (VarID[ID] = VarType.size());
    if (VarID.size() > 23)
      emitError("SymbolTable", "number of variables exceed 23");
    VarType.push_back((DeclType == DECL_INT ? VAR_INT : VAR_FLOAT));
    return Res;
  }

  size_t getVarID(std::string ID) const {
    auto Iter = VarID.find(ID);
    if (Iter == VarID.end())
      emitError("SymbolTable", "use of undeclared identifier ", ID);
    return Iter->second;
  }

  VariableType getVarType(std::string ID) const { return VarType[getVarID(ID)]; }
  VariableType getVarType(size_t V) const { return VarType[V]; }
};

class Parser {
  Tokenizer TK;
  SymbolTable ST;
  AST *parseStatement();
  AST *parseAssignment(std::string ID);
  AST *parseDeclaration(TokenType DeclType);
  AST *parseValue();
  AST *parseParenExpression(int &paramCount);
  AST *parseExpression(AST *LHS, int &paramCount);
  int constantFolding(AST *Node, AST *LHS, AST *RHS);

  DataType getDataType(AST *Node) const;
  DataType promoteType(AST *&LHS, AST *&RHS) const;

public:
  Parser(std::ifstream &&S) : TK(std::move(S)) {}
  const SymbolTable &getSymbolTable() const { return ST; }
  AST *parse();
};

AST *Parser::parse() {
  AST *Node = new AST(PROGRAM_NODE);
  bool EndOfDecl = false, FirstStmt = true;
  while (TK) {
    AST *Stmt = parseStatement();
    if (!FirstStmt && !TK.hasReadWhiteSpaces())
      emitError("Parser", "expecting white spaces at the end of a statement.");
    FirstStmt = false;
    if (EndOfDecl && Stmt->Type == DECLARATION_NODE)
      emitError("Parser",
                "declarations should come before other types of statements.");
    if (Stmt->Type != DECLARATION_NODE)
      EndOfDecl = true;
    Node->SubTree.push_back(Stmt);
  }
  return Node;
}

AST *Parser::parseStatement() {
  Token Tok = TK.readToken();
  if (Tok.Type == DECL_INT || Tok.Type == DECL_FLOAT)
    return parseDeclaration(Tok.Type);

  if (Tok.Type == IDENTIFIER) {
    return parseAssignment(Tok.Value);
  }

  if (Tok.Type != CMD_PRINT)
    emitError("Parser",
              "expecting DECL_INT, DECL_FLOAT, IDENTIFIER or CMD_PRINT, but ",
              Tok.Type, " found.");
  Tok = TK.readToken();
  if (Tok.Type != IDENTIFIER)
    emitError("Parser", "expecting IDENTIFIER, but ", Tok.Type, " found.");

  // Print statement.
  size_t Var = ST.getVarID(Tok.Value);
  AST *Node = new AST(PRINTSTMT_NODE);
  Node->Value = Var;
  return Node;
}

AST *Parser::parseDeclaration(TokenType DeclType) {
  Token Tok = TK.readToken();
  if (Tok.Type != IDENTIFIER)
    emitError("Parser", "expecting IDENTIFIER, but ", DeclType, " found.");
  AST *Node = new AST(DECLARATION_NODE);
  Node->Value = ST.declareSymbol(Tok.Value, DeclType);
  return Node;
}

AST *Parser::parseAssignment(std::string ID) {
  size_t Var = ST.getVarID(ID);
  if (TokenType T = TK.readToken().Type; T != BIN_OP_ASSIGN)
    emitError("Parser", "expecting BIN_OP_ASSIGN, but ", T, " found.");

  int paramCount = 0;
  AST *LHS;
  if (TK.peekToken().Type == LEFT_PARAM) {
    ++paramCount;
    TK.readToken();
    LHS = parseParenExpression(paramCount);
  }
  else if (TK.peekToken().Type == IDENTIFIER || TK.peekToken().Type == CONST_INT || TK.peekToken().Type == CONST_FLOAT){
    LHS = parseValue();
  }
  else {
    emitError("Parser", "expecting LEFT_PARAM, IDENTIFIER, CONST_INT, CONST_FLOAT, but ", TK.peekToken().Type, " found.");
  }
  AST *Expr = parseExpression(LHS, paramCount);
  if (paramCount != 0) {
    emitError("Parser", "unmatched parentheses");
  }

  if (getDataType(Expr) == DATA_FLOAT && ST.getVarType(Var) == VAR_INT)
    emitError("Parser", "cannot convert float to integer.");
  AST *Node = new AST(ASSIGNMENT_NODE, Expr);
  Node->Value = Var;
  return Node;
}
AST *Parser::parseParenExpression(int &paramCount) {
  if (TK.peekToken().Type == RIGHT_PARAM) {
    emitError("Parser", "No expression in parentheses pair");
  }
  if (TK.peekToken().Type == LEFT_PARAM) {
    TK.readToken();
    ++paramCount;
    AST *LHS = parseParenExpression(paramCount);
    AST *Expr = parseExpression(LHS, paramCount);
    return Expr;
  }
  else {
    AST *LHS = parseValue();
    AST *Expr = parseExpression(LHS, paramCount);
    return Expr;
  }
}

DataType Parser::getDataType(AST *Node) const {
  if (Node->Type == CONST_INT_NODE)
    return DATA_INT;
  if (Node->Type == CONST_FLOAT_NODE)
    return DATA_FLOAT;
  if (Node->Type == IDENTIFIER_NODE)
    return ST.getVarType(std::get<size_t>(Node->Value)) == VAR_INT ? DATA_INT
                                                                   : DATA_FLOAT;
  if (Node->Type == BIN_ADD_NODE || Node->Type == BIN_SUB_NODE || Node->Type == BIN_MUL_NODE || Node->Type == BIN_DIV_NODE)
    return std::get<DataType>(Node->Value);

  emitError("Parser", "unexpected AST node type.");
}

DataType Parser::promoteType(AST *&LHS, AST *&RHS) const {
  DataType L = getDataType(LHS);
  DataType R = getDataType(RHS);
  if (L == R)
    return L;

  AST *Cvt = new AST(CONVERSION_NODE);
  Cvt->Value = DATA_FLOAT;
  if (L == DATA_INT) {
    Cvt->SubTree = {LHS};
    LHS = Cvt;
  } else {
    Cvt->SubTree = {RHS};
    RHS = Cvt;
  }
  return DATA_FLOAT;
}

AST *Parser::parseValue() {
  Token Tok = TK.readToken();
  AST *Node = new AST();
  switch (Tok.Type) {
  case CONST_INT:
    Node->Type = CONST_INT_NODE;
    Node->Value = std::stoi(Tok.Value);
    break;
  case CONST_FLOAT:
    Node->Type = CONST_FLOAT_NODE;
    Node->Value = std::stof(Tok.Value);
    break;
  case IDENTIFIER:
    Node->Type = IDENTIFIER_NODE;
    Node->Value = ST.getVarID(Tok.Value);
    break;
  default:
    emitError("Parser", "expecting CONST_INT, CONST_FLOAT or IDENTIFIER, but ",
              Tok.Type, " found.");
  }
  return Node;
}

int Parser::constantFolding(AST *Node, AST *LHS, AST *RHS) {
  if (LHS->Type == CONST_INT_NODE && RHS->Type == CONST_INT_NODE) {
    int leftInt = std::get<int>(LHS->Value);
    int rightInt = std::get<int>(RHS->Value);
    int folded;
    if (Node->Type == BIN_ADD_NODE) {
      folded = leftInt + rightInt;
    }
    else if (Node->Type == BIN_SUB_NODE) {
      folded = leftInt - rightInt;
    }
    else if (Node->Type == BIN_MUL_NODE) {
      folded = leftInt * rightInt;
    }
    else if (Node->Type == BIN_DIV_NODE) {
      folded = leftInt / rightInt;
    }
    Node->Type = CONST_INT_NODE;
    Node->Value = folded;
    return 1;
  }
  if ((LHS->Type == CONST_FLOAT_NODE && RHS->Type == CONST_FLOAT_NODE)
      || (LHS->Type == CONST_FLOAT_NODE && RHS->Type == CONST_INT_NODE)
      || (LHS->Type == CONST_FLOAT_NODE && RHS->Type == CONVERSION_NODE)
      || (LHS->Type == CONST_INT_NODE && RHS->Type == CONST_FLOAT_NODE)
      || (LHS->Type == CONVERSION_NODE && RHS->Type == CONST_FLOAT_NODE)) {

    float leftFloat;
    if (LHS->Type == CONST_FLOAT_NODE) {
      leftFloat = std::get<float>(LHS->Value);
    }
    else if (LHS->Type == CONST_INT_NODE) {
      leftFloat = static_cast<float>(std::get<int>(LHS->Value));
    }
    else if (LHS->Type == CONVERSION_NODE) {
      leftFloat = static_cast<float>(std::get<int>(LHS->SubTree[0]->Value));
    }
    else {
      __builtin_unreachable();
    }
    
    float rightFloat;
    if (RHS->Type == CONST_FLOAT_NODE) {
      rightFloat = std::get<float>(RHS->Value);
    }
    else if (RHS->Type == CONST_INT_NODE) {
      rightFloat = static_cast<float>(std::get<int>(RHS->Value));
    }
    else if (RHS->Type == CONVERSION_NODE) {
      rightFloat = static_cast<float>(std::get<int>(RHS->SubTree[0]->Value));
    }
    else {
      __builtin_unreachable();
    }

    float folded;
    if (Node->Type == BIN_ADD_NODE) {
      folded = leftFloat + rightFloat;
    }
    else if (Node->Type == BIN_SUB_NODE) {
      folded = leftFloat - rightFloat;
    }
    else if (Node->Type == BIN_MUL_NODE) {
      folded = leftFloat * rightFloat;
    }
    else if (Node->Type == BIN_DIV_NODE) {
      folded = leftFloat / rightFloat;
    }
    Node->Type = CONST_FLOAT_NODE;
    Node->Value = folded;
    return 1;
  }
  return 0;
}

AST *Parser::parseExpression(AST *LHS, int &paramCount) {
  if (TK.peekToken().Type == RIGHT_PARAM) {
    --paramCount;
    /* strip right parentheses */
    TK.readToken();
    return LHS;
  }
  if (TK.peekToken().Type != BIN_OP_ADD && TK.peekToken().Type != BIN_OP_SUB
      && TK.peekToken().Type != BIN_OP_MUL && TK.peekToken().Type != BIN_OP_DIV) {
    return LHS;
  }

  ASTNodeType nodeType;
  switch (TK.peekToken().Type) {
    case BIN_OP_ADD:
      nodeType = BIN_ADD_NODE;
      break;
    case BIN_OP_SUB:
      nodeType = BIN_SUB_NODE;
      break;
    case BIN_OP_MUL:
      nodeType = BIN_MUL_NODE;
      break;
    case BIN_OP_DIV:
      nodeType = BIN_DIV_NODE;
      break;
    default:
    __builtin_unreachable();
  }
  AST *Node = new AST(nodeType);
  TK.readToken();

  /*
   * Check whether the right hand side expression consists of parentheses
   */
  AST *RHS;
  if (TK.peekToken().Type == LEFT_PARAM) {
    TK.readToken();
    ++paramCount;
    RHS = parseParenExpression(paramCount);
  }
  else {
    RHS = parseValue();
  }
  
  if (TK.peekToken().Type == BIN_OP_MUL || TK.peekToken().Type == BIN_OP_DIV) {
    AST *LHS2 = RHS;
    while (TK.peekToken().Type == BIN_OP_MUL || TK.peekToken().Type == BIN_OP_DIV) {
      /* 
       * Create multiplication or division node
       */

      TokenType Type2 = TK.peekToken().Type;
      AST *Node2 = new AST(Type2 == BIN_OP_MUL ? BIN_MUL_NODE : BIN_DIV_NODE);
      TK.readToken();

      /*
       * Check whether the right hand side expression consists of parentheses
       */
      AST *RHS2;
      if (TK.peekToken().Type == LEFT_PARAM) {
        TK.readToken();
        ++paramCount;
        RHS2 = parseParenExpression(paramCount);
      }
      else {
        RHS2 = parseValue();
      }
      Node2->Value = promoteType(LHS2, RHS2);

      /* constand folding */

      if (!constantFolding(Node2, LHS2, RHS2)) {
        Node2->SubTree = {LHS2, RHS2};
      }
      LHS2 = Node2;
    }
    RHS = LHS2;
  }
  Node->Value = promoteType(LHS, RHS);

  /* constant folding */

  if (!constantFolding(Node, LHS, RHS)) {
    Node->SubTree = {LHS, RHS};
  }
  return parseExpression(Node, paramCount);
}

class DCCodeGen {
  const SymbolTable &ST;
  std::ofstream OFS;
  enum Precision { PREC_INT, PREC_FLOAT, PREC_UNKNOWN };
  Precision CurPrec = PREC_UNKNOWN;

  void genStatement(AST *Stmt);
  void genAssignment(AST *Stmt);
  void genExpression(AST *Expr);
  void genPrintStmt(AST *Stmt);

public:
  DCCodeGen(const SymbolTable &ST, std::ofstream &&S)
      : ST(ST), OFS(std::move(S)) {
    OFS << std::fixed << std::setprecision(5);
  }

  void genProgram(AST *Program);
};

void DCCodeGen::genProgram(AST *Program) {
  assert(Program->Type == PROGRAM_NODE);
  for (AST *Node : Program->SubTree)
    genStatement(Node);
}

void DCCodeGen::genStatement(AST *Stmt) {
  if (Stmt->Type == ASSIGNMENT_NODE)
    return genAssignment(Stmt);
  if (Stmt->Type == PRINTSTMT_NODE)
    return genPrintStmt(Stmt);
}

void DCCodeGen::genAssignment(AST *Stmt) {
  assert(Stmt->SubTree.size() == 1);
  AST *Expr = Stmt->SubTree[0];
  genExpression(Expr);
  OFS << "s" << static_cast<char>('a' + std::get<size_t>(Stmt->Value)) << "\n";
}

inline char printOperator(ASTNodeType Type) {
  switch (Type) {
    case BIN_ADD_NODE:
      return '+';
    case BIN_SUB_NODE:
      return '-';
    case BIN_MUL_NODE:
      return '*';
    case BIN_DIV_NODE:
      return '/';
    default:
      __builtin_unreachable();
  }
}

void DCCodeGen::genExpression(AST *Expr) {
  switch (Expr->Type) {
  case CONVERSION_NODE:
    return genExpression(Expr->SubTree[0]);
  case IDENTIFIER_NODE:
    OFS << "l" << static_cast<char>('a' + std::get<size_t>(Expr->Value))
        << "\n";
    return;
  case CONST_INT_NODE:
    OFS << std::get<int>(Expr->Value) << "\n";
    return;
  case CONST_FLOAT_NODE:
    OFS << std::get<float>(Expr->Value) << "\n";
    return;
  case BIN_ADD_NODE:
  case BIN_SUB_NODE: 
  case BIN_MUL_NODE:
  case BIN_DIV_NODE: {
    assert(Expr->SubTree.size() == 2 &&
           "Expecting two children for binary expressions.");
    genExpression(Expr->SubTree[0]);
    genExpression(Expr->SubTree[1]);
    Precision Prec =
        std::get<DataType>(Expr->Value) == DATA_INT ? PREC_INT : PREC_FLOAT;
    if (Prec != CurPrec) {
      OFS << (Prec == PREC_INT ? 0 : 5) << " k\n";
      CurPrec = Prec;
    }
    OFS << printOperator(Expr->Type) << "\n";
    return;
  }
  default:
    emitError("CodeGen", "unexpected AST node type.");
  }
}

void DCCodeGen::genPrintStmt(AST *Stmt) {
  assert(Stmt->SubTree.empty());
  OFS << "l" << static_cast<char>('a' + std::get<size_t>(Stmt->Value)) << "\n";
  OFS << "p\n";
}

int main(int argc, const char **argv) {
  if (argc != 3) {
    std::cerr << "[Usage] " << argv[0] << " source_file target_file\n";
    exit(1);
  }
  std::ifstream Source(argv[1]);
  if (!Source) {
    std::cerr << "[Error] failed to open source_file\n";
    exit(1);
  }
  std::ofstream Target(argv[2]);
  if (!Target) {
    std::cerr << "[Error] failed to open target_file\n";
    exit(1);
  }
  Parser PS(std::move(Source));
  AST *Program = PS.parse();
  DCCodeGen DCG(PS.getSymbolTable(), std::move(Target));
  DCG.genProgram(Program);
  delete Program;
  return 0;
}
