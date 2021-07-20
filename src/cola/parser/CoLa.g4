grammar CoLa;


/* Parser rules: programs */

program : opts* struct_decl* defContains defInvariant var_decl* function* EOF ;

opts : '#' ident=Identifier str=String  #option ;

struct_decl : ('struct' || 'class') name=Identifier '{' field_decl* '}' (';')? ;

typeName : VoidType    #nameVoid
         | BoolType    #nameBool
         | IntType     #nameInt
         | DataType    #nameData
         | Identifier  #nameIdentifier
         ;

type : name=typeName      #typeValue
     | name=typeName '*'  #typePointer
     ;

typeList : types+=type
         | '(' types+=type (',' types+=type)+ ')'
         | '<' types+=type (',' types+=type)+ '>'
         ;

field_decl : type names+=Identifier (',' names+=Identifier)* ';' ;

var_decl : type name=Identifier '@root' ';'                     #varDeclRoot
         | type names+=Identifier (',' names+=Identifier)* ';'  #varDeclList
         ;

function :          types=typeList name=Identifier args=argDeclList body=scope (';')?  #functionInterface
         | 'inline' types=typeList name=Identifier args=argDeclList body=scope (';')?  #functionMacro
         |          VoidType       '__init__'      '(' ')'          body=scope (';')?  #functionInit
         ;

argDeclList : '(' (argTypes+=type argNames+=Identifier (',' argTypes+=type argNames+=Identifier)*)? ')' ;

block : statement  #blockStmt
      | scope      #blockScope
      ;

scope : '{' var_decl* statement* '}' ;

statement : 'choose' scope+                                               #stmtChoose
          | 'loop' scope                                                  #stmtLoop
          | 'atomic' body=block                                           #stmtAtomic
          | 'if' '(' expr=expression ')' bif=block ('else' belse=block)?  #stmtIf
          | 'while' '(' expr=expression ')' body=block                    #stmtWhile
          | 'do' body=block 'while' '(' expr=expression ')' ';'           #stmtDo
          | command ';'                                                   #stmtCom
          ;

command : 'skip'                                           #cmdSkip
        | lhs=expression '=' rhs=expression                #cmdAssign
        | lhs=Identifier '=' 'malloc'                      #cmdMalloc
        | 'assume' '(' expr=expression ')'                 #cmdAssume
        | 'assert' '(' expr=expression ')'                 #cmdAssert
        | (lhs=argList '=')? name=Identifier args=argList  #cmdCall
        | 'continue'                                       #cmdContinue
        | 'break'                                          #cmdBreak
        | 'return'                                         #cmdReturnVoid
        | 'return' expr=expression                         #cmdReturnSingle
        | 'return' args=argList                            #cmdReturnList
        | cas                                              #cmdCas
        ;

// TODO: singleton argList should not have parenthesis
argList : '(' (arg+=expression (',' arg+=expression)*)? ')'
        | '<' (arg+=expression (',' arg+=expression)*)? '>'
        ;

cas : 'CAS' '(' dst=expression ',' cmp=expression ',' src=expression ')'  #casSingle
    | 'CAS' '(' dst=argList ',' cmp=argList ',' src=argList ')'           #casMultiple
    ;

value : Null    #valueNull
      | True    #valueTrue
      | False   #valueFalse
      | Ndet    #valueNDet
      | Empty   #valueEmpty
      | Maxval  #valueMax
      | Minval  #valueMin
      ;

expression : name=Identifier                        #exprIdentifier
           | value                                  #exprValue
           | '(' expr=expression ')'                #exprParens
           | expr=expression '->' field=Identifier  #exprDeref
           | Neg expr=expression                    #exprNegation
           | lhs=expression Eq rhs=expression       #exprBinaryEq
           | lhs=expression Neq rhs=expression      #exprBinaryNeq
           | lhs=expression Lt rhs=expression       #exprBinaryLt
           | lhs=expression Lte rhs=expression      #exprBinaryLte
           | lhs=expression Gt rhs=expression       #exprBinaryGt
           | lhs=expression Gte rhs=expression      #exprBinaryGte
           | lhs=expression And rhs=expression      #exprBinaryAnd
           | lhs=expression Or rhs=expression       #exprBinaryOr
           | cas                                    #exprCas
           ;

defContains : ; // TODO
defInvariant : ; // TODO


/* Lexer rules */

Inline : 'inline' ;
Extern : 'extern' ;

VoidType : 'void' ;
DataType : 'data_t' ;
BoolType : 'bool' ;
IntType  : 'int' ;

Null   : 'NULL' ;
Empty  : 'EMPTY' ;
True   : 'true' ;
False  : 'false' ;
Ndet   : '*' ;
Maxval : 'MAX_VAL' ;
Minval : 'MIN_VAL' ;

Eq  : '==' ;
Neq : '!=' ;
Lt  : '<' ;
Lte : '<=' ;
Gt  : '>' ;
Gte : '>=' ;
And : '&&' ;
Or  : '||' ;

Neg : '!' ;

Identifier : Letter ( ('-' | '_')* (Letter | '0'..'9') )* ;
fragment Letter : [a-zA-Z] ;
// Identifier : [a-zA-Z][a-zA-Z0-9_]* ;

String : '"' ~[\r\n]* '"'
       | '\'' ~[\r\n]* '\'' ;

WS    : [ \t\r\n]+ -> skip ; // skip spaces, tabs, newlines

COMMENT      : '/*' .*? '*/' -> channel(HIDDEN) ;
LINE_COMMENT : '//' ~[\r\n]* -> channel(HIDDEN) ;

// UNMATCHED          : .  -> channel(HIDDEN);
