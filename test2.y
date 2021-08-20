%token NUM
%%
goal: expr { printf("%d\n", $$); };
expr: expr '+' term {
	$$ = $1 + $3;
	printf("%d = %d + %d\n", $$, $1, $3);
};
expr: expr '-' term {
	$$ = $1 - $3;
	printf("%d = %d - %d\n", $$, $1, $3);
};
expr: term;
term: term '*' factor {
	$$ = $1 * $3;
	printf("%d = %d * %d\n", $$, $1, $3);
};
term: term '/' factor {
	$$ = $1 / $3;
	printf("%d = %d * %d\n", $$, $1, $3);
};
term: factor;
factor: '(' expr ')';
factor: NUM;

%%

int _tok[] = { 256, '+', 256, '*', 256, 0 };
int _lvals[] = { 111, '+', 222, '*', 333, 0 };
int *tok = _tok;
int *lval = _lvals;
                                
int
yylex(void)
{
	yylval = *lval++;
	return (*tok++);
}

int
main(void)
{
	return (yyparse());
}

