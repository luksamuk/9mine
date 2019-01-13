#include <u.h>
#include <libc.h>
#include <bio.h>

/* Play field */
#define FIELD_SIDE 16
#define FIELD_MAX_LETTER 'p'
#define NUM_BOMBS  40
#define FIELD_NUM_CELLS (FIELD_SIDE * FIELD_SIDE)

static int field[FIELD_NUM_CELLS];
static int reveal[FIELD_NUM_CELLS];
static int flags[FIELD_NUM_CELLS];

/* Used due to flattening a 2D array into 1D */
#define coord_to_idx(y, x)      ((y * FIELD_SIDE) + x)

/* Accessors for matrices */
#define field_cell(y, x)   field[coord_to_idx(y, x)]
#define reveal_cell(y, x)  reveal[coord_to_idx(y, x)]
#define flags_cell(y, x)   flags[coord_to_idx(y, x)]

/* Predicate for whether a cell has been revealed */
#define is_revealed(y, x)  (reveal_cell(y, x) != 0)

/* Globals */
static int DEBUG = 0;
static int QUIT  = 0;

/* Identifiers on field */
#define NOTHING 0
#define BOMB    9

/* Identifiers on flags */
#define FLAG    1
#define DOUBT   2


/* Checking for bombs around a specific cell */

int
is_bomb_at(int y, int x)
{
    if((y < 0 || y >= FIELD_SIDE) ||
       (x < 0 || x >= FIELD_SIDE))
        return 0;

    return (field_cell(y, x) == BOMB) ? 1 : 0;
}

int
num_neighbour_bombs(int y, int x)
{
    int num_bombs = 0;
    num_bombs += is_bomb_at(y, x - 1);     // west
    num_bombs += is_bomb_at(y, x + 1);     // east
    num_bombs += is_bomb_at(y - 1, x);     // north
    num_bombs += is_bomb_at(y - 1, x - 1); // northwest
    num_bombs += is_bomb_at(y - 1, x + 1); // northeast
    num_bombs += is_bomb_at(y + 1, x);     // south
    num_bombs += is_bomb_at(y + 1, x - 1); // southwest
    num_bombs += is_bomb_at(y + 1, x + 1); // southeast
    return num_bombs;
}

/* Field output */

void
print_horizontal_ruler(void)
{
    int i;
    print("   ");
    for(i = 0; i < FIELD_SIDE; i++)
        print("%03d ", i + 1);
    print("\n");
}

void
print_vertical_letter(char letter, int side)
{
    print((!side) ? "%c |" : "| %c", letter);
}

void
print_field_godmode(void)
{
    int i, j;
    print_horizontal_ruler();
    for(i = 0; i < FIELD_SIDE; i++) {
        print_vertical_letter('A' + (char) i, 0);
        for(j = 0; j < FIELD_SIDE; j++) {
            print(" ");
            switch(field_cell(i, j)) {
            case NOTHING: print(" "); break;
            case BOMB:    print("*"); break;
            default:
                print("%d", field_cell(i, j));
                break;
            }
            print("  ");
        }
        print_vertical_letter('A' + (char) i, 1);
        print("\n");
    }
    print_horizontal_ruler();
}

void
print_field(void)
{
    int i, j;
    print_horizontal_ruler();
    for(i = 0; i < FIELD_SIDE; i++) {
        print_vertical_letter('A' + (char) i, 0);
        for(j = 0; j < FIELD_SIDE; j++) {
            print(" ");
            if(is_revealed(i, j))
                switch(field_cell(i, j)) {
                case NOTHING: print(" "); break;
                case BOMB:    print("*"); break;
                default:
                    print("%d", field_cell(i, j));
                    break;
                }
            else
                switch(flags_cell(i, j)) {
                case FLAG:  print("%c", '%'); break;
                case DOUBT: print("?");       break;
                default:
                    if(!QUIT || field_cell(i, j) != BOMB)
                        print("-");
                    else print("*");
                    break;
                }
            print("  ");
        }
        print_vertical_letter('A' + (char) i, 1);
        print("\n");
    }
    print_horizontal_ruler();
}

/* Initialize game */
int
random_number(void)
{
    return rand() % FIELD_SIDE;
}

void
init_game(void)
{
    memset(field,  0, FIELD_NUM_CELLS * sizeof(int));
    memset(reveal, 0, FIELD_NUM_CELLS * sizeof(int));
    memset(flags,  0, FIELD_NUM_CELLS * sizeof(int));

    /* Put bombs at random places */
    int i;
    for(i = 0; i < NUM_BOMBS; i++) {
        int line, column;
        do {
            line   = random_number();
            column = random_number();
        } while(field_cell(line, column) == BOMB);
        field_cell(line, column) = BOMB;
    }

    /* Populate other cells with numbers of bombs nearby */
    int j;
    for(i = 0; i < FIELD_SIDE; i++)
        for(j = 0; j < FIELD_SIDE; j++)
            if(field_cell(i, j) != BOMB)
                field_cell(i, j) = num_neighbour_bombs(i, j);
}


/* Input & REPL */
static Biobuf bstdin;

int
get_coord_input(void)
{
    int line, column;
    print("Input line letter:   ");
    
    {
        char c;

        do {
            c = Bgetc(&bstdin);
        } while(c == '\n' || c == '\t' || c == '\r');
        
        line = ((int) c) - ((int) 'a');
    }

    Bflush(&bstdin);
    
    print("Input column number: ");

    {
        // atoi
        char *str;
        str = Brdline(&bstdin, 10);
        int len = Blinelen(&bstdin) - 1;
        if(str == 0)
            return -1;
        str[len] = '\0';
        column = atoi(str) - 1;
    }

    if((line < 0 || column < 0) ||
       (line >= FIELD_SIDE || column >= FIELD_SIDE))
        return -1;

    return coord_to_idx(line, column);
}

int
toggle_mark(int type)
{
    int coord;
    coord = get_coord_input();
    if(coord < 0) {
        print("Invalid input.\n");
        return 1;
    } else if(reveal[coord] != 0) {
        print("Cannot mark a revealed place.");
        return 1;
    }

    if(flags[coord] == type)
        flags[coord] = 0;
    else flags[coord] = type;
    return 0;
}

void
propagate(int y, int x)
{
    if((y < 0 || y >= FIELD_SIDE) ||
       (x < 0 || x >= FIELD_SIDE) ||
       is_revealed(y, x))
        return;

    reveal_cell(y, x) = 1;
    if(field_cell(y, x) == 0) {
        propagate(y, x - 1); // west
        propagate(y - 1, x - 1); // northwest
        propagate(y - 1, x); // north
        propagate(y - 1, x + 1); // northeast
        propagate(y, x + 1); // east
        propagate(y + 1, x + 1); // southeast
        propagate(y + 1, x); // south
        propagate(y + 1, x - 1); // southwest
    }
}

int
step(void)
{
    int coord;
    coord = get_coord_input();
    if(coord < 0) {
        print("Invalid input.\n");
        return 1;
    } else if(reveal[coord] != 0) {
        print("You happily waltz at the meadows...\n");
        return 1;
    } else if(flags[coord] != 0) {
        print("You know you shouldn't step at a marked place.\n");
        return 1;
    } else if(field[coord] == BOMB) {
        print("You stepped on a mine! Game over.\n");
        QUIT = 1;
        return 0;
    }

    int y, x;
    x = coord % FIELD_SIDE;
    y = (int) floor((coord - x) / FIELD_SIDE);
    propagate(y, x);
    return 0;
}

void
repl(void)
{
    int print_godmode = 0;
    int should_print  = 1;
    do {
        /* Field printing changes depending on debug mode */
        if(should_print) {
            if(DEBUG && print_godmode) {
                print_field_godmode();
                print_godmode = 0;
            }
            else print_field();
        }
        should_print = 1;

        /* Command prompt and input */
        print(DEBUG ? "(s)tep, (f)lag, (d)oubt, (q)uit, (g)odview? "
              : "(s)tep, (f)lag, (d)oubt, (q)uit? ");

        /* Read character, ignore whitespaces */
        char ch;
        
        do {
            ch = Bgetc(&bstdin);
        } while(ch == '\n' || ch == '\t' || ch == '\r');

        Bflush(&bstdin);

        switch(ch) {
        case Beof:
        case 'q': QUIT = 1;                           break;
        case 'f': should_print = !toggle_mark(FLAG);  break;
        case 'd': should_print = !toggle_mark(DOUBT); break;
        case 's': should_print = !step();             break;
        case 'g':
            if(DEBUG) {
                print_godmode = 1;
                break;
            }
        default:
            print("Unknown command.\n");
            should_print = 0;
            break;
        }
        print("\n");
    } while(!QUIT);
    print_field();
}



void
main(int argc, char** argv)
{
    srand(time(0));

    /* Detect debug flag */
    ARGBEGIN {
        case 'd':
            DEBUG = 1;
            break;
    } ARGEND
    

    /* Initialize input */
    if(Binit(&bstdin, 0, OREAD) == Beof) {
        fprint(2, "Error connecting stdin to bio: %r");
        exits("Binit");
    }


    init_game();
    repl();

    
    Bterm(&bstdin);
    exits(0);
}
