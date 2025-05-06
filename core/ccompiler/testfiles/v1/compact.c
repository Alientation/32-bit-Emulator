/*
EXPECTED AST

prog
-> func <ident "main"> " () {" <body> "}"
    -> statement "return " <body> ";"
        -> expr <body>
            -> factor <body>
                -> literal_int "2"
*/


int main (){return 2;}