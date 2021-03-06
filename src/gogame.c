/* Go game
 *
 * Author: Christoph Hermes (hermes@hausmilbe.net)
 */

#include "gogame.h"

#include <stdlib.h>
#include <assert.h>

#include <sgftree.h>
#include <inkview.h>

#include "goboard.h"

/******************************************************************************/

typedef struct {
    char *name;
    char *rank;
} Player;

typedef struct {
    Player black;
    Player white;
    int boardSize;
    char *komi;
    int handicap;
    char *date;
    char *result;
    int time;
    char *overtime;
    char *ruleset;
} GameInfo;

typedef struct {
    int fontSize;
    int fontSpace;
    ifont *font_ttf;
    int info_y; /* beginning of comment and variation window */
    int comment_width; /* width of comment window */
    int varFontSize;
    int varFontSep;
    ifont *varWin_ttf;
    int varwin_w; /* max number of moves shown */
    int varwin_h; /* max number of variations shown */
    int border_sep; /* distance to screen border */
} DrawProperties;

/******************************************************************************/

static SGFTree *gameTree = NULL; /* game tree */
static SGFNode *curNode = NULL; /* current node in the game tree */

static char *comment_str = NULL;
static int comment_update = 0;

static GameInfo gameInfo;

static DrawProperties drawProps;

static char *str_unknown = "unknown";

static int bShowFullScreenComment = 0;
static int bShowHelpScreen = 0;

/******************************************************************************/

#define GET_CHAR_PROP(name__, ref__) \
    if (!sgfGetCharProperty(gameTree->root, name__, &ref__)) \
        ref__ = str_unknown;

#define ENC_SGFPROP(c1_, c2_) ((short)( c1_ | c2_ << 8 ))

/******************************************************************************/

void readGameInfo();
void initDrawProperties();
void test_readSGF();
void debug_msg(char *s);
void apply_sgf_cmds_to_board();
void updateCommentStr();

/******************************************************************************/

void debug_msg(char *s) 
{/*{{{*/
    ifont *times12;

    times12 = OpenFont("DejaVuSerif", 12, 1);
    FillArea(350, 770, 250, 20, WHITE);
    SetFont(times12, BLACK);
    DrawString(350, 770, s);
    PartialUpdateBW(350, 770, 250, 20);
    CloseFont(times12);
}/*}}}*/

int gogame_new_from_file(const char *filename)
{/*{{{*/
    gogame_cleanup();
    initDrawProperties();

    gameTree = (SGFTree *) malloc(sizeof(SGFTree));
    if (gameTree == NULL)
        return 1;
    sgftree_clear(gameTree); /* set node pointers to NULL */

    if (!sgftree_readfile(gameTree, filename)) {
        gogame_cleanup();
        return 2;
    }
    curNode = gameTree->root;

    readGameInfo();

    board_new(gameInfo.boardSize, drawProps.fontSize * 2 + drawProps.fontSpace * 3);

    apply_sgf_cmds_to_board();
    /* test_readSGF(); */

    updateCommentStr();
    bShowFullScreenComment = 0;
    bShowHelpScreen = 0;

    return 0;
}/*}}}*/

void gogame_cleanup()
{/*{{{*/
    if (gameTree != NULL) {
        /* free SGF info */
        sgfFreeNode(gameTree->root); /* recursively free the sgf tree */
        sgftree_clear(gameTree);
        free(gameTree);
        gameTree = NULL;
        curNode = NULL;

        /* cleanup other game info */
        gameInfo.black.name = NULL;
        gameInfo.black.rank = NULL;
        gameInfo.white.name = NULL;
        gameInfo.white.rank = NULL;
        gameInfo.boardSize  = 19;
        gameInfo.komi       = NULL;
        gameInfo.handicap   = 0;
        gameInfo.date       = NULL;
        gameInfo.result     = NULL;
        gameInfo.time       = 0;
        gameInfo.overtime   = NULL;
        gameInfo.ruleset    = NULL;

        /* cleanup draw properties */
        drawProps.fontSize = 12;
        CloseFont(drawProps.font_ttf);
        drawProps.font_ttf = NULL;

        drawProps.varFontSize = 12;
        drawProps.varFontSep = 0;
        drawProps.varwin_h = 0;
        drawProps.varwin_w = 0;
        CloseFont(drawProps.varWin_ttf);
        drawProps.varWin_ttf = NULL;

        bShowFullScreenComment = 0;
        bShowHelpScreen = 0;

        /* cleanup go board */
        board_cleanup();
    }
}/*}}}*/

void initDrawProperties()
{/*{{{*/
    drawProps.fontSize  = (int) ((double)ScreenWidth() / 600.0 * 14.0);
    drawProps.fontSpace = (int) ((double)ScreenWidth() / 600.0 * 4.0);
    drawProps.font_ttf = OpenFont("DejaVuSerif", drawProps.fontSize, 1);

    /* variation window */
    drawProps.varwin_w = 4;
    drawProps.varwin_h = 6;

    drawProps.varFontSize  = (int) ((double)ScreenWidth() / 600.0 * 20.0);
    drawProps.varFontSep = drawProps.varFontSize / 4;
    drawProps.varWin_ttf = OpenFont("drocerog", drawProps.varFontSize, 1);

    /* distance to screen border */
    drawProps.border_sep = drawProps.varFontSize;

    /* comment window */
    drawProps.info_y = drawProps.fontSize * 2 + drawProps.fontSpace * 2
                            + ScreenWidth();
    drawProps.comment_width = ScreenWidth()
                              - drawProps.varwin_w * 2 * drawProps.varFontSize + drawProps.varFontSize      /* varWin width */
                              - 3 * drawProps.border_sep;                                                   /* space between screen and components */
}/*}}}*/

void test_readSGF()
{/*{{{*/
    SGFNode *cur = NULL;
    SGFProperty *prop = NULL;
    int r, c;

    /* test: list all properties in the main path, ignoring children */
    for (cur = gameTree->root; cur; cur = cur->child) {
        /* list all properties */
        for (prop = cur->props; prop; prop = prop->next) {
            fprintf(stderr, "%c%c[%s] ",
                prop->name & 255, prop->name >> 8, prop->value );
        }
        fprintf(stderr, "\n");

        for (prop = cur->props; prop; prop = prop->next) {
            r = get_moveX(prop, gameInfo.boardSize);
            c = get_moveY(prop, gameInfo.boardSize);
            switch (prop->name) {
                case ENC_SGFPROP('A', 'W'):
                    board_placeStone(r, c, BOARD_WHITE, 0);
                    break;

                case ENC_SGFPROP('A', 'B'):
                    board_placeStone(r, c, BOARD_BLACK, 0);
                    break;

                case ENC_SGFPROP('B', ' '):
                    board_placeStone(r, c, BOARD_BLACK, 1);
                    break;

                case ENC_SGFPROP('W', ' '):
                    board_placeStone(r, c, BOARD_WHITE, 1);
                    break;
            }
        }
    }
}/*}}}*/

void draw_variation(int bPartialUpdate)
{/*{{{*/
    int i, lvl, x, y, x_parent, y_parent;
    SGFNode *nd = NULL;
    SGFNode *ndVar = NULL;
    SGFNode *ndBegin = NULL;
    char *tmp;
    char gInfo[256];
    int caps_b, caps_w;

    if (!gameTree)
        return;
    if (!curNode)
        return;

    if (bPartialUpdate) {
        FillArea(drawProps.comment_width + 2 * drawProps.border_sep,                 /* x */
                 drawProps.info_y,                                                   /* y */
                 ScreenWidth() - drawProps.comment_width + 2 * drawProps.border_sep, /* w */
                 ScreenHeight() - drawProps.info_y,                                  /* h */
                 WHITE);
    }

    /* print game info (move number, captured stones) */
    SetFont(drawProps.font_ttf, BLACK);
    board_get_captured(&caps_b, &caps_w);
    snprintf(gInfo, sizeof(gInfo), "Move %d\nCap.: B[%d] W[%d]", curNode->move_num, caps_b, caps_w);
    DrawTextRect(drawProps.comment_width + 2 * drawProps.border_sep,                 /* x */
                 drawProps.info_y,                                                   /* y */
                 ScreenWidth() - drawProps.comment_width + 2 * drawProps.border_sep, /* w */
                 drawProps.fontSize * 3,
                 gInfo, ALIGN_LEFT | VALIGN_TOP );

    /* find top variation */
    for (ndBegin=curNode; ndBegin->prevVar; ndBegin=ndBegin->prevVar) {};

    /* begin variation overview with the previous move */
    if (ndBegin->parent)
        ndBegin = ndBegin->parent;

    i = 0;
    for (nd=ndBegin; nd; nd=nd->child) {
        for (ndVar=nd; ndVar; ndVar=ndVar->nextVar) {
            lvl = ndVar->draw_lvl;
            x = drawProps.comment_width + 2 * drawProps.border_sep + 2 * i * drawProps.varFontSize;
            y = ScreenHeight() - drawProps.varFontSize - drawProps.varFontSize * drawProps.varFontSep 
                + lvl * (drawProps.varFontSize + drawProps.varFontSep);

            if (lvl < drawProps.varwin_h) {
                /* draw "parent" line */
                if (ndVar->parent && i > 0) {
                    x_parent = drawProps.comment_width + 2 * drawProps.border_sep + 2 * (i-1) * drawProps.varFontSize;
                    y_parent = ScreenHeight() - drawProps.varFontSize - drawProps.varFontSize * drawProps.varFontSep 
                               + ndVar->parent->draw_lvl * (drawProps.varFontSize + drawProps.varFontSep);

                    DrawLine(x, y + drawProps.varFontSize / 2,
                             x_parent + drawProps.varFontSize, y_parent + drawProps.varFontSize / 2, 
                             BLACK);
                }

                if (is_move_node(ndVar)) {
                    /* set current position color */
                    SetFont(drawProps.varWin_ttf, BLACK);

                    /* draw stone */
                    if (sgfGetCharProperty(ndVar, "B ", &tmp)) {
                        DrawString(x, y, "K");
                        SetFont(drawProps.varWin_ttf, WHITE);
                    } else if (sgfGetCharProperty(ndVar, "W ", &tmp)) {
                        DrawString(x, y, "L");
                        SetFont(drawProps.varWin_ttf, BLACK);
                    }

                    /* indicate comment if exists */
                    if (sgfGetCharProperty(ndVar, "C ", &tmp))
                        DrawString(x, y, "O");
                } else { /* no move: draw just a placeholder */
                    /* set position color */
                    SetFont(drawProps.varWin_ttf, BLACK);

                    /* draw triangle */
                    SetFont(drawProps.varWin_ttf, BLACK);
                    DrawString(x, y, "O");
                }

                /* indicate current position */
                if (ndVar == curNode) {
                    DrawLine(x, y, x + drawProps.varFontSize, y, BLACK);
                    DrawLine(x + drawProps.varFontSize, y, x + drawProps.varFontSize, y + drawProps.varFontSize, BLACK);
                    DrawLine(x, y + drawProps.varFontSize, x + drawProps.varFontSize, y + drawProps.varFontSize, BLACK);
                    DrawLine(x, y, x, y + drawProps.varFontSize, BLACK);
                }
            }
        }

        i += 1;
        if (i >= drawProps.varwin_w)
            break;
    }

    if (bPartialUpdate) {
        PartialUpdateBW(drawProps.comment_width + 2 * drawProps.border_sep,                 /* x */
                        drawProps.info_y,                                                   /* y */
                        ScreenWidth() - drawProps.comment_width + 2 * drawProps.border_sep, /* w */
                        ScreenHeight() - drawProps.info_y);                                 /* h */
    }

}/*}}}*/

void gogame_draw_fullrepaint()
{/*{{{*/
    char msg[1024];
    ifont *default_ttf;
    int curFontSz, linePts;

    // fprintf(stderr, "gogame.c: gogame_draw_fullrepaint() called\n");

    ClearScreen();

    if (!bShowHelpScreen && gameTree != NULL) {
        if (!bShowFullScreenComment) {
            /* draw title / header */
            SetFont(drawProps.font_ttf, BLACK);
            snprintf( msg, sizeof(msg),
                "Black: %s [%s], White: %s [%s], Date: %s, Result: %s",
                gameInfo.black.name, gameInfo.black.rank, gameInfo.white.name, gameInfo.white.rank, 
                gameInfo.date, gameInfo.result);
            DrawString(drawProps.border_sep, drawProps.fontSpace, msg);
            snprintf( msg, sizeof(msg),
                "Time: %d min (%s), Komi: %s, Handicap: %d, Ruleset: %s",
                gameInfo.time / 60, gameInfo.overtime,
                gameInfo.komi, gameInfo.handicap, gameInfo.ruleset);
            DrawString(drawProps.border_sep, drawProps.fontSpace*2+drawProps.fontSize, msg);

            /* draw comment window */
            if (comment_str != NULL) {
                SetFont(drawProps.font_ttf, BLACK);
                DrawTextRect( drawProps.border_sep, drawProps.info_y,
                              drawProps.comment_width, ScreenHeight() - drawProps.info_y,
                              comment_str,
                              ALIGN_LEFT | VALIGN_TOP );
                comment_update = 0;
                // fprintf(stderr, "x, y = %d, %d | w, h = %d, %d\n", 5, drawProps.info_y,
                        // drawProps.comment_width, ScreenHeight() - drawProps.info_y);
            }

            /* draw variation window */
            draw_variation(0);

        } else { /* if (bShowFullScreenComment) */
            assert(comment_str != NULL);

            SetFont(drawProps.font_ttf, BLACK);
            DrawTextRect( drawProps.border_sep, drawProps.border_sep,
                          ScreenWidth() - 2 * drawProps.border_sep,
                          ScreenHeight() - 3 * drawProps.border_sep,
                          comment_str,
                          ALIGN_LEFT | VALIGN_TOP );
            DrawString(drawProps.border_sep,
                       ScreenHeight() - 2 * drawProps.border_sep,
                       "Info: Press the OK button to switch back to the game.");
        }

    } else {
        curFontSz = ScreenWidth() / 600 * 20;
        linePts = curFontSz; /* free line */

        /* title */
        default_ttf = OpenFont("DejaVuSerif", curFontSz, 1);
        SetFont(default_ttf, BLACK);
        DrawString(drawProps.border_sep, linePts, "droceRoG - Go Game Record Viewer");
        linePts += curFontSz + curFontSz / 2;
        CloseFont(default_ttf);

        linePts += curFontSz + curFontSz / 2; /* free line */

        /* Author, version and short help */
        curFontSz = ScreenWidth() / 600 * 14;
        default_ttf = OpenFont("DejaVuSerif", curFontSz, 1);
        SetFont(default_ttf, BLACK);
        DrawString(drawProps.border_sep, linePts, "Author: Christoph Hermes (hermes<at>hausmilbe<dot>net)");
        linePts += curFontSz + curFontSz / 2;
        DrawString(drawProps.border_sep, linePts, "Version: "DROCEROG_VERSION);
        linePts += curFontSz + curFontSz / 2;
        linePts += curFontSz + curFontSz / 2; /* free line */
        DrawString(drawProps.border_sep, linePts, "Please open a file by pressing the (context) menu symbol on the right side.");
        linePts += curFontSz + curFontSz / 2;

        linePts += curFontSz + curFontSz / 2; /* free line */

        /* Longer help */
        DrawTextRect(drawProps.border_sep, linePts,
                     ScreenWidth() - 2 * drawProps.border_sep, ScreenHeight() - linePts,
                     "This program reads a SGF file (Smart Go/Game Format) and displays the contents on the screen of your PocketBook reader. It is useful for studying a (commented) Go game but useless if you want to play against the computer. You will find more detailed information and the sources on \n\n      http://drocerog.hausmilbe.net\n\n\
Send me a message if you have any suggestions, like to contribute, or just want to tell me how awesome this tool is. Have fun playing and studying Go!\n\
\n\
\n\
Right-hand keys:\n\
\n\
* Home - Exits the program and returns to PocketBook intro screen.\n\
* Menu - Opens context menu (file selection, go to move, etc.)\n\
* Forward / Backward - One move forward / backward\n\
* OK - Displays a comment on the full screen instead under the board\n\
\n\
\n\
Navigation keys:\n\
\n\
* Left / Right - Move to previous / next variation or comment\n\
* Up / Down - Switch between variations\n\
* Return - Exit program",
                     ALIGN_LEFT | VALIGN_TOP);

        /* Notification how to get back to the game */
        if (bShowHelpScreen) {
            DrawString(drawProps.border_sep,
                       ScreenHeight() - 2 * drawProps.border_sep,
                       "Info: Press the OK button to switch back to the game.");
        }

        CloseFont(default_ttf);
    }

    /* draw go board, if an SGF is loaded and none fullscreen info has to be
     * displayed */
    if (gameTree != NULL && !bShowFullScreenComment && !bShowHelpScreen)
        board_draw_update(0);

    FullUpdate();

}/*}}}*/

void gogame_draw_update()
{/*{{{*/
    // fprintf(stderr, "gogame.c: gogame_draw_update called\n");
    if (!gameTree)
        return;

    board_draw_update(1);

    if (comment_update) {
        FillArea(drawProps.border_sep, drawProps.info_y,
                 drawProps.comment_width, ScreenHeight() - drawProps.info_y,
                 WHITE);
        if (comment_str != NULL) {
            SetFont(drawProps.font_ttf, BLACK);
            DrawTextRect( drawProps.border_sep, drawProps.info_y,
                          drawProps.comment_width, ScreenHeight() - drawProps.info_y,
                          comment_str,
                          ALIGN_LEFT | VALIGN_TOP );
            // fprintf(stderr, "x, y = %d, %d | w, h = %d, %d\n", 5, drawProps.info_y,
                    // drawProps.comment_width, ScreenHeight() - drawProps.info_y);
        } 

        PartialUpdate(drawProps.border_sep, drawProps.info_y,
                      drawProps.comment_width, ScreenHeight() - drawProps.info_y);
        comment_update = 0;
    }

    draw_variation(1);
}/*}}}*/

void updateCommentStr()
{/*{{{*/
    char *msg; 
    // char *ptr1, *ptr2;

    assert(gameTree != NULL);
    assert(curNode != NULL);

    if (!sgfGetCharProperty(curNode, "C ", &msg))
        msg = NULL;

    if (msg == NULL) {
        if (comment_str != NULL) {
            comment_str = msg;
            comment_update = 1;
            return;
        } else {
            comment_update = 0;
            return;
        }
    } else {
        comment_str = msg;
        comment_update = 1;

        // /* replace double '\n' occurrences by spaces */
        // for (ptr1 = strstr(msg, "\n"); ptr1; ptr1 = strstr(ptr1+1, "\n")) {
            // ptr2 = ptr1 + 1;
            // while (*ptr2 == ' ')
                // ptr2 += 1;
            // if (*ptr2 == '\n')
                // *ptr1 = ' ';
        // }
    }

}/*}}}*/

void gogame_printGameInfo()
{/*{{{*/
    if (gameTree == NULL)
        return;

    fprintf(stderr, "GAME INFO   Black: %s [%s], White: %s [%s]\n", gameInfo.black.name, gameInfo.black.rank, gameInfo.white.name, gameInfo.white.rank);
    fprintf(stderr, "            Board size %d x %d\n", gameInfo.boardSize, gameInfo.boardSize);
    fprintf(stderr, "            Result = %s, Date = %s\n", gameInfo.result, gameInfo.date);
    fprintf(stderr, "            Komi = %s, Handicap = %d\n", gameInfo.komi, gameInfo.handicap);
    fprintf(stderr, "            Ruleset = %s, Time = %d min, Overtime = %s\n", gameInfo.ruleset, gameInfo.time/60, gameInfo.overtime);
}/*}}}*/

void readGameInfo()
{/*{{{*/
    assert(gameTree != NULL);

    GET_CHAR_PROP("PB", gameInfo.black.name);

    GET_CHAR_PROP("PB", gameInfo.black.name);
    GET_CHAR_PROP("BR", gameInfo.black.rank);
    GET_CHAR_PROP("PW", gameInfo.white.name);
    GET_CHAR_PROP("WR", gameInfo.white.rank);

    if (!sgfGetIntProperty(gameTree->root, "SZ", &gameInfo.boardSize))
        gameInfo.boardSize = 19;
    GET_CHAR_PROP("KM", gameInfo.komi);
    if (!sgfGetIntProperty(gameTree->root, "HA", &gameInfo.handicap))
        gameInfo.handicap = 0;
    GET_CHAR_PROP("DT", gameInfo.date);
    GET_CHAR_PROP("RE", gameInfo.result);
    if (!sgfGetIntProperty(gameTree->root, "TM", &gameInfo.time))
        gameInfo.time = 0;
    GET_CHAR_PROP("OT", gameInfo.overtime);
    GET_CHAR_PROP("RU", gameInfo.ruleset);
}/*}}}*/

void gogame_move_forward_update(int bUpdate)
{/*{{{*/
    if (gameTree == NULL)
        return;
    if (bShowFullScreenComment) /* disable motion while fullscreen comment */
        return;

    /* do nothing, if no continuation in this variation exists */
    if (!curNode->child) 
        return;
    curNode = curNode->child;

    apply_sgf_cmds_to_board();

    if (bUpdate)
        updateCommentStr();
}/*}}}*/
void gogame_move_forward()
{/*{{{*/
    gogame_move_forward_update(1);
}/*}}}*/

void apply_sgf_cmds_to_board()
{/*{{{*/
    SGFProperty *prop = NULL;
    int sz = 0;

    assert(gameTree != NULL);

    sz = gameInfo.boardSize;

    /* for all properties in this move */
    for (prop = curNode->props; prop; prop = prop->next) {
        switch (prop->name) {

            case ENC_SGFPROP('A', 'B'):     /* added black stone */
                board_placeStone(get_moveX(prop, sz), get_moveY(prop, sz), BOARD_BLACK, 0);
                break;
            case ENC_SGFPROP('A', 'W'):     /* added white stone */
                board_placeStone(get_moveX(prop, sz), get_moveY(prop, sz), BOARD_WHITE, 0);
                break;

            case ENC_SGFPROP('B', ' '):     /* move: black stone */
                board_placeStone(get_moveX(prop, sz), get_moveY(prop, sz), BOARD_BLACK, 1);
                break;
            case ENC_SGFPROP('W', ' '):     /* move: white stone */
                board_placeStone(get_moveX(prop, sz), get_moveY(prop, sz), BOARD_WHITE, 1);
                break;

            case ENC_SGFPROP('S', 'Q'):     /* marker: square */
                board_placeMarker(get_moveX(prop, sz), get_moveY(prop, sz), MARK_SQUARE);
                break;
            case ENC_SGFPROP('C', 'R'):     /* marker: circle */
                board_placeMarker(get_moveX(prop, sz), get_moveY(prop, sz), MARK_CIRC);
                break;
            case ENC_SGFPROP('T', 'R'):     /* marker: triangle */
                board_placeMarker(get_moveX(prop, sz), get_moveY(prop, sz), MARK_TRIANGLE);
                break;
        }
    }
}/*}}}*/

void gogame_move_back_update(int bUpdate)
{/*{{{*/
    if (gameTree == NULL)
        return;
    if (bShowFullScreenComment) /* disable motion while fullscreen comment */
        return;

    if (board_undo())
        curNode = curNode->parent;

    if (bUpdate)
        updateCommentStr();
}/*}}}*/
void gogame_move_back()
{/*{{{*/
    gogame_move_back_update(1);
}/*}}}*/

void undo_variation(SGFNode *srcNode, SGFNode *targetNode)
{/*{{{*/
    SGFNode *srcNode_i, *targetNode_i;
    SGFNode *pathToTarget = NULL;

    assert(srcNode);
    assert(targetNode);

    srcNode_i = srcNode;
    targetNode_i = targetNode;

    /* Find same parent, undo path to srcNode, and record path to targetNode */
    while (srcNode_i && targetNode_i && srcNode_i != targetNode_i) {
        if (board_undo())
            curNode = curNode->parent;

        srcNode_i = srcNode_i->parent;

        if (pathToTarget == NULL) {
            pathToTarget = sgfNewNode();
            /* HACK: use "next" pointer as data pointer */
            pathToTarget->next = targetNode_i;  
        } else {
            assert( pathToTarget->parent == NULL );
            pathToTarget->parent = sgfNewNode();
            pathToTarget->parent->child = pathToTarget;
            /* HACK: use "next" pointer as data pointer */
            pathToTarget->parent->next = targetNode_i;  
            pathToTarget = pathToTarget->parent;
        }
        targetNode_i = targetNode_i->parent;
    }
    assert(srcNode_i != NULL);      /* this should never happen */
    assert(targetNode_i != NULL);

    /* reverse path to target */
    for (targetNode_i=pathToTarget; targetNode_i; targetNode_i=targetNode_i->child) {
        curNode = targetNode_i->next;
        targetNode_i->next = NULL; /* this prevents being freed at the end */
        apply_sgf_cmds_to_board();
    }

    /* free path */
    sgfFreeNode(pathToTarget);

}/*}}}*/

void gogame_moveVar_down()
{/*{{{*/
    SGFNode *ndCur = NULL;
    SGFNode *ndNextVar = NULL;
    int lvl;

    if (gameTree == NULL)
        return;
    if (bShowFullScreenComment) /* disable motion while fullscreen comment */
        return;

    /* go to beginning of variations */
    ndCur = curNode;
    while (ndCur->prevVar)
        ndCur = ndCur->prevVar;

    /* determine node with higher lvl value next to current */
    lvl = 20000;
    for (; ndCur; ndCur=ndCur->nextVar) {
        if (ndCur->draw_lvl > curNode->draw_lvl && ndCur->draw_lvl < lvl) {
            lvl = ndCur->draw_lvl;
            ndNextVar = ndCur;
        }
    }

    if (ndNextVar == NULL)
        return;

    undo_variation(curNode, ndNextVar);

    updateCommentStr();
}/*}}}*/

void gogame_moveVar_up()
{/*{{{*/
    SGFNode *ndCur = NULL;
    SGFNode *ndPrevVar = NULL;
    int lvl;

    if (gameTree == NULL)
        return;
    if (bShowFullScreenComment) /* disable motion while fullscreen comment */
        return;

    /* go to beginning of variations */
    ndCur = curNode;
    while (ndCur->prevVar)
        ndCur = ndCur->prevVar;

    /* determine node with lower lvl value next to current */
    lvl = -20000;
    for (; ndCur; ndCur=ndCur->nextVar) {
        if (ndCur->draw_lvl < curNode->draw_lvl && ndCur->draw_lvl > lvl) {
            lvl = ndCur->draw_lvl;
            ndPrevVar = ndCur;
        }
    }

    if (ndPrevVar == NULL)
        return;

    undo_variation(curNode, ndPrevVar);

    updateCommentStr();
}/*}}}*/

void gogame_move_to_nextEvt()
{/*{{{*/
    char *msg;

    if (gameTree == NULL)
        return;
    if (bShowFullScreenComment) /* disable motion while fullscreen comment */
        return;

    /* inital step forward */
    gogame_move_forward_update(0);

    /* move forward until end, variation, or comment is reached */
    while (curNode->child && curNode->next == NULL && !sgfGetCharProperty(curNode, "C ", &msg)) {
        gogame_move_forward_update(0);
    }

    /* update comment */
    updateCommentStr();
}/*}}}*/

void gogame_move_to_prevEvt()
{/*{{{*/
    char *msg;

    if (gameTree == NULL)
        return;
    if (bShowFullScreenComment) /* disable motion while fullscreen comment */
        return;

    /* inital step backward */
    gogame_move_back_update(0);

    /* move backward until beginning, variation, or comment is reached */
    while (curNode->parent && curNode->next == NULL && curNode->parent->child->next == NULL && !sgfGetCharProperty(curNode, "C ", &msg)) {
        gogame_move_back_update(0);
    }

    /* update comment */
    updateCommentStr();
}/*}}}*/

int gogame_move_to_page(int page)
{/*{{{*/
    if (gameTree == NULL)
        return 0;
    if (bShowFullScreenComment) /* disable motion while fullscreen comment */
        return 0;
    if (curNode == NULL)
        return 0;

    /* check if we are already on desired page */
    if (page == curNode->move_num)
        return 0;

    /* check if you have to move forward or backward */
    if (page < curNode->move_num) {
        /* move backward until beginning or page is reached */
        while (curNode->parent && page < curNode->move_num)
            gogame_move_back_update(0);
    } else if (page > curNode->move_num) {
        /* move forward until end or page is reached */
        while (curNode->child && page > curNode->move_num)
            gogame_move_forward_update(0);
    }

    /* update comment */
    updateCommentStr();

    return 1;

}/*}}}*/

int gogame_switch_fullComment()
{/*{{{*/
    if (gameTree == NULL)
        return 0;
    if (comment_str == NULL)
        return 0;

    bShowFullScreenComment = !bShowFullScreenComment;
    return 1;
}/*}}}*/

int gogame_isGameOpened()
{/*{{{*/
    if (gameTree == NULL)
        return 0;
    else
        return 1;
}/*}}}*/

int gogame_set_showHelp(int bShowHelp)
{/*{{{*/
    int oldVal = bShowHelpScreen;

    bShowHelpScreen = bShowHelp;

    return oldVal;
}/*}}}*/

int gogame_isHelpShown()
{/*{{{*/
    return bShowHelpScreen;
}/*}}}*/
