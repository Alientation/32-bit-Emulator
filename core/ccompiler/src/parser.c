#include "ccompiler/parser.h"

#include <stdio.h>
#include <stdlib.h>


static void ASTNode_free (void *ast_node);

int parse (struct LexerData *lexer,
           struct ParserData *parser)
{
    return 0;
}


struct ParserData parser_init (struct LexerData lexer)
{
    struct ParserData parser;
    parser.lexer = lexer;
    parser.tok_i = 0;
    parser.ast = NULL;
    return parser;
}

void parser_free (struct ParserData *parser)
{
    ASTNode_free (parser->ast);
}


static void ASTNode_free (void *node)
{
    astnode_t *ast_node = (astnode_t *) node;
    switch (ast_node->type)
    {
        case AST_ERROR:
            perror ("Encountered AST_ERROR node while freeing.");
            break;
        case AST_PROGRAM:
            ASTNode_free (((astprogram_t *) ast_node)->function );
            break;
        case AST_EXPRESSION:
            break;
        case AST_STATEMENT:
            ASTNode_free (((aststatement_t *) ast_node)->expression );
            break;
        case AST_FUNCTION:
            ASTNode_free (((astfunction_t *) ast_node)->statement );
            break;
    }

    free (ast_node);
}