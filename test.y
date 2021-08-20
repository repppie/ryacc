%token LPAR RPAR 

%%

goal: list;
list: list pair { $$ = $1; };
list: pair;
pair: LPAR pair RPAR { $$ = $1 + $2; };
pair: LPAR RPAR;
