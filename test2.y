%token NUM
%%
goal: expr;
expr: expr '+' term;
expr: expr '-' term;
expr: term;
term: term '*' factor;
term: term '/' factor;
term: factor;
factor: '(' expr ')';
factor: NUM;
