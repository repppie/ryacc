%token LPAR RPAR 

%%

goal: list;
list: list pair;
list: pair;
pair: LPAR pair RPAR;
pair: LPAR RPAR;
