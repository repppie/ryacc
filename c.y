%token ID CONSTANT STRING SIZEOF
%token PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP MUL_ASS DIV_ASS MOD_ASS ADD_ASS
%token SUB_ASS LEFT_ASS RIGHT_ASS AND_ASS XOR_ASS OR_ASS TYPE_NAME
%token TYPEDEF EXTERN STATIC AUTO REGISTER
%token CHAR SHORT INT LONG SIGNED UNSIGNED FLOAT DOUBLE CONST VOLATILE VOID
%token STRUCT UNION ENUM ELLIPSIS
%token CASE DEFAULT IF ELSE SWITCH WHILE DO FOR GOTO CONTINUE BREAK RETURN

%%

goal: expr;

id: ID;
constant: CONSTANT;

prim_expr: id | constant;
prim_expr: STRING;
prim_expr: '(' expr ')';

postfix_expr: prim_expr;
postfix_expr: postfix_expr '[' expr ']';
postfix_expr: postfix_expr '(' ')';
postfix_expr: postfix_expr '(' arg_expr_list ')';
postfix_expr: postfix_expr '.' ID;
postfix_expr: postfix_expr PTR_OP ID;
postfix_expr: postfix_expr INC_OP ID;
postfix_expr: postfix_expr DEC_OP ID;

arg_expr_list: ass_expr;
arg_expr_list: arg_expr_list ',' ass_expr;

unary_expr: postfix_expr;
unary_expr: INC_OP unary_expr;
unary_expr: DEC_OP unary_expr;
unary_expr: unary_op cast_expr;
unary_expr: SIZEOF unary_expr;
unary_expr: SIZEOF '(' unary_expr ')';

unary_op: '&';
unary_op: '*';
unary_op: '+';
unary_op: '-';
unary_op: '~';
unary_op: '!';

cast_expr: unary_expr;
cast_expr: '(' TYPE_NAME ')' cast_expr;

mult_expr: cast_expr;
mult_expr: mult_expr '*' cast_expr;
mult_expr: mult_expr '/' cast_expr;
mult_expr: mult_expr '%' cast_expr;

add_expr: mult_expr;
add_expr: add_expr '+' mult_expr;
add_expr: add_expr '-' mult_expr;

shift_expr: add_expr;
shift_expr: shift_expr LEFT_OP add_expr;
shift_expr: shift_expr RIGHT_OP add_expr;

rela_expr: shift_expr;
rela_expr: rela_expr '<' shift_expr;
rela_expr: rela_expr '>' shift_expr;
rela_expr: rela_expr LE_OP shift_expr;
rela_expr: rela_expr GE_OP shift_expr;

eql_expr: rela_expr;
eql_expr: eql_expr EQ_OP rela_expr;
eql_expr: eql_expr NE_OP rela_expr;

and_expr: eql_expr;
and_expr: and_expr '&' eql_expr;

xor_expr: and_expr;
xor_expr: xor_expr '^' and_expr;

ior_expr: xor_expr;
ior_expr: ior_expr '|' xor_expr;

land_expr: ior_expr;
land_expr: land_expr AND_OP ior_expr;

lor_expr: land_expr;
lor_expr: lor_expr OR_OP ior_expr;

cond_expr: lor_expr;
cond_expr: lor_expr '?' expr ':' cond_expr;

ass_expr: cond_expr;
ass_expr: unary_expr ass_op ass_expr;

ass_op: '=';
ass_op: MUL_ASS;
ass_op: DIV_ASS;
ass_op: MOD_ASS;
ass_op: ADD_ASS;
ass_op: SUB_ASS;
ass_op: LEFT_ASS;
ass_op: RIGHT_ASS;
ass_op: AND_ASS;
ass_op: XOR_ASS;
ass_op: OR_ASS;

expr: ass_expr;
expr: expr ',' ass_expr;
