#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <windows.h>
#include <time.h>
#include <ctype.h>

/* --- CONFIGURAZIONE GRIGLIA --- */
#define WIDTH 60
#define HEIGHT 20

/* --- DEFINIZIONE COLORI CONSOLE (Attributi Windows) --- */
#define COLOR_WALL    11    // Ciano chiaro
#define COLOR_SNAKE   10    // Verde
#define COLOR_AI      13    // Magenta
#define COLOR_FOOD    12    // Rosso
#define COLOR_GOLD    14    // Giallo/Oro
#define COLOR_POISON  13    // Viola/Veleno
#define COLOR_DEFAULT 7     // Bianco standard

/* --- STRUTTURE DATI --- */
typedef struct { 
    int x, y; 
} Point;

// Nodo per algoritmi di ricerca (A* / Dijkstra)
typedef struct { 
    Point p; 
    int g_score;    // Costo dal punto di partenza
    int f_score;    // Costo totale stimato (g + h)
    int prev_idx;   // Indice del nodo precedente per ricostruire il cammino
} Node;

/* --- VARIABILI GLOBALI --- */
Point snake[1000];          // Array corpo serpente
Point food, special;        // Posizioni cibo
Point exit_door = {0, 10};  // Coordinate portale di fuga
int length, direction;      // Stato movimento
int score, lives;           // Statistiche sessione
int is_ai;                  // Flag modalità (0: Player, 1: AI)
int ai_mode = 0;            // Sottotipo AI (1: Dijkstra, 2: A*)
int special_type;           // Tipo bonus attivo (1: Oro, 2: Veleno)
int door_open = 0;          // Flag sblocco varco uscita
int flash_state = 0;        // Toggle per lampeggio UI
int offset_x = 0;           // Offset per centratura orizzontale
Node queue[WIDTH * HEIGHT]; // Coda per calcolo pathfinding

/* --- PROTOTIPI FUNZIONI --- */
// Gestione Schermo e Colore
void gotoxy(int x, int y);
void set_color(int color);
void hide_cursor();
void update_offset();
int get_window_width();
void print_menu_line(char* text);

// Logica AI e Calcolo
int get_dist(Point p1, Point p2);
int get_ai_direction();

// Rendering e Interfaccia
void draw_ui();
int show_menu();
void show_info();
void draw_border();

// Ciclo di Gioco
void logic();
void setup_round();


////////////////////////////////////////////////////////////////////////////////
/* --- FUNZIONI DI GESTIONE CONSOLE E COORDINATE --- */

// Recupera la larghezza attuale della finestra del terminale (Windows)
int get_window_width() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.dwSize.X;
    }
    return 80; // Valore di fallback standard
}

// Calcola l'offset orizzontale per centrare il campo di gioco (WIDTH) nella finestra
void update_offset() {
    int w = get_window_width();
    offset_x = (w - WIDTH) / 2;
    if (offset_x < 0) offset_x = 0; // Impedisce offset negativi su finestre piccole
}

// Posiziona il cursore alle coordinate (x,y) considerando l'offset di centratura
void gotoxy(int x, int y) {
    COORD coord = { (SHORT)(x + offset_x), (SHORT)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

// Stampa una riga di testo centrata nel terminale basandosi su una larghezza fissa (80)
void print_menu_line(char* text) {
    int screen_w = get_window_width();
    int art_width = 80; 
    int padding = (screen_w - art_width) / 2;
    if (padding < 0) padding = 0;

    for (int i = 0; i < padding; i++) printf(" ");
    printf("%s\n", text);
}

// Cambia l'attributo colore del testo per le stampe successive
void set_color(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

// Rende invisibile il cursore della console per evitare sfarfallii durante il rendering
void hide_cursor() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info = {100, FALSE};
    SetConsoleCursorInfo(h, &info);
}


/* --- FUNZIONI MATEMATICHE E DI CALCOLO --- */

// Calcola la distanza di Manhattan tra due punti (usata per l'euristica A*)
int get_dist(Point p1, Point p2) {
    return abs(p1.x - p2.x) + abs(p1.y - p2.y);
}
/* --- MOTORE DI INTELLIGENZA ARTIFICIALE --- */
// Calcola la prossima mossa ottimale utilizzando Dijkstra o A*
int get_ai_direction() {
    Point head_p = snake[0];
    
    // Determinazione dell'obiettivo prioritario: Uscita > Bonus Oro > Cibo standard
    Point target = (door_open) ? exit_door : ((special_type == 1) ? special : food);
    
    // Inizializzazione griglia logica per collisioni (static per efficienza memoria)
    static int grid[WIDTH + 1][HEIGHT + 2];
    for(int i=0; i<=WIDTH; i++) for(int j=0; j<=HEIGHT+1; j++) grid[i][j] = 0;
    
    // Definizione perimetro e ostacoli fissi
    for(int i=0; i<=WIDTH; i++) { grid[i][1] = 1; grid[i][HEIGHT+1] = 1; }
    for(int j=1; j<=HEIGHT+1; j++) { grid[0][j] = 1; grid[WIDTH][j] = 1; }
    
    // Mappatura del corpo dello snake come area invalicabile
    for(int i=1; i<length; i++) {
        if(snake[i].x >= 0) grid[snake[i].x][snake[i].y] = 1;
    }
    
    // Gestione ostacoli dinamici e varchi
    if (special_type == 2) grid[special.x][special.y] = 1; // Evita il Veleno (P)
    if (door_open) grid[exit_door.x][exit_door.y] = 0;      // Sblocca il varco d'uscita

    // Inizializzazione BFS/A*
    int q_head = 0, q_tail = 0;
    int start_f = (ai_mode == 2) ? get_dist(head_p, target) : 0;
    queue[q_tail++] = (Node){head_p, 0, start_f, -1};
    grid[head_p.x][head_p.y] = 1; // Segna la partenza come visitata

    // Vettori di movimento: Su, Giù, Sinistra, Destra
    int dx[] = {0, 0, -1, 1}, dy[] = {-1, 1, 0, 0};

    /* FASE 1: RICERCA PERCORSO OTTIMALE */
    while(q_head < q_tail) {
        // Se A* è attivo, seleziona il nodo con il minor costo stimato (Best-First)
        if (ai_mode == 2) {
            int best = q_head;
            for(int i = q_head + 1; i < q_tail; i++) {
                if(queue[i].f_score < queue[best].f_score) best = i;
            }
            Node temp = queue[q_head]; queue[q_head] = queue[best]; queue[best] = temp;
        }

        Node curr = queue[q_head++];
        
        // Target raggiunto: ricostruzione del cammino a ritroso
        if(curr.p.x == target.x && curr.p.y == target.y) {
            Node temp = curr;
            while(temp.prev_idx != -1) {
                // Identifica la prima mossa necessaria per avviare il cammino
                if(queue[temp.prev_idx].prev_idx == -1) {
                    if(temp.p.y < head_p.y) return 72; // SU
                    if(temp.p.y > head_p.y) return 80; // GIU
                    if(temp.p.x < head_p.x) return 75; // SINISTRA
                    if(temp.p.x > head_p.x) return 77; // DESTRA
                }
                temp = queue[temp.prev_idx];
            }
        }

        // Espansione dei nodi adiacenti (Scansione vicinato)
        for(int i=0; i<4; i++) {
            int nx = curr.p.x + dx[i], ny = curr.p.y + dy[i];
            if(nx >= 0 && nx <= WIDTH && ny >= 1 && ny <= HEIGHT + 1 && !grid[nx][ny]) {
                grid[nx][ny] = 1;
                int g = curr.g_score + 1;
                int h = (ai_mode == 2) ? get_dist((Point){nx, ny}, target) : 0;
                queue[q_tail++] = (Node){{nx, ny}, g, g + h, q_head - 1};
            }
        }
    }

    /* FASE 2: PROTOCOLLO DI EVASIONE (FAILSAFE) */
    // Eseguito se il target è circondato o irraggiungibile. Cerca la prima mossa libera.
    for(int i=0; i<4; i++) {
        int nx = head_p.x + dx[i], ny = head_p.y + dy[i];
        
        if(nx > 0 && nx < WIDTH && ny > 1 && ny <= HEIGHT) {
            int collision = 0;
            // Verifica collisione corpo
            for(int j=1; j<length; j++) {
                if(nx == snake[j].x && ny == snake[j].y) { collision = 1; break; }
            }
            
            // Verifica integrità perimetro/varco
            if(!door_open && nx == exit_door.x && ny == exit_door.y) collision = 1;
            
            // Prevenzione inversione a U istantanea (Self-collision immediata)
            if(i == 0 && direction == 80) collision = 1;
            if(i == 1 && direction == 72) collision = 1;
            if(i == 2 && direction == 77) collision = 1;
            if(i == 3 && direction == 75) collision = 1;

            if(!collision) {
                if(i == 0) return 72;
                if(i == 1) return 80;
                if(i == 2) return 75;
                if(i == 3) return 77;
            }
        }
    }

    return direction; // Mantieni direzione attuale se non esistono vie di fuga
}
/* --- INTERFACCIA UTENTE E RENDERING DEI MENU --- */

// Renderizza la barra di stato superiore (Score, Vite, Messaggi porta)
void draw_ui() {
    set_color(COLOR_DEFAULT);
    gotoxy(0, 0);
    
    // Messaggio prioritario: Portale di uscita attivo
    if(score >= 500) {
        set_color(flash_state ? COLOR_GOLD : COLOR_FOOD);
        printf(" !!! PORTA APERTA: VAI A SINISTRA !!!          ");
    } 
    // Info specifiche per le modalità AI
    else if(is_ai) {
        set_color(COLOR_DEFAULT);
        printf(" AI: %s | SCORE: %d / 500 | 'Q' MENU          ", (ai_mode == 1 ? "DIJKSTRA" : "A*"), score);
    } 
    // Info per il giocatore umano: Score e barra della vita cromatica
    else {
        set_color(COLOR_DEFAULT);
        printf(" SCORE: %-3d / 500 | VITE: ", score);
        
        for(int i = 0; i < 3; i++) {
            // I cuori cambiano colore da Rosso (attivo) a Grigio (perso)
            if(i < lives) {
                set_color(COLOR_FOOD); 
                printf("<3 ");         
            } else {
                set_color(COLOR_DEFAULT); 
                printf("<3 ");
            }
        }
        printf(" "); // Buffer di pulizia riga
    }
}

// Visualizza il manuale operativo e le istruzioni di gioco
void show_info() {
    system("cls");
    set_color(COLOR_SNAKE);
    // ASCII Art "Snake Man" con allineamento manuale del testo informativo
    printf("          ___ \n");
    printf("        ,-----, \n");
    printf("       /\\|   |/\\         MANUALE OPERATIVO:\n");
    printf("      |-- \\_/ --|         ------------------------------------------\n");
    printf("   .-----/   \\-----.      [ COMANDI ] \n");
    printf("  /   ,   . .   ,   \\     Frecce / WASD: Muovi lo Snake\n");
    printf(" /  /`|    |    |'\\, \\    Tasto 'Q' (in AI): Torna al Menu\n");
    printf(" `\\ \\  \\-  |  -/  /`/'\n");
    printf("   `\\\\_)`-- --'(_//       [ OBIETTIVO MISSIONE ]\n");
    printf("     |_|`-- --'|_|  _______    Raggiungi 500 PUNTI per aprire il varco\n");
    printf("      ,'`-   -'`.,-'       `-. nel muro sinistro (X:0, Y:10) e FUGGI!\n");
    printf("     |\\--------/||            `-.\n");
    printf("    |\\---------/`|    .--.       `----'   ___--.`--.  [ AI INFO ]\n");
    printf("     |\\---------/\\. .\"    `.            ,'      `---' Dijkstra: Breadth-First\n");
    printf("      ``-._______.-'        `-._______.-'               A*: Heuristic Search\n\n");
    
    set_color(COLOR_GOLD);
    printf("    PREMI UN TASTO PER TORNARE AL MENU...");
    _getch();
}

// Gestisce il menu principale e la selezione delle modalità
int show_menu() {
    system("cls"); 
    hide_cursor(); 
    update_offset(); 
    
    int screen_w = get_window_width();
    int art_width = 80;
    int padding = (screen_w - art_width) / 2;
    if (padding < 0) padding = 0;

    // --- DISEGNO COBRA E TITOLO COLORATO ---
    set_color(COLOR_SNAKE);
    printf("%*s      ---_ ......._-_--. \n", padding, "");
    printf("%*s     (|\\ /      / /| \\  \\ \n", padding, "");

    // Da qui iniziamo a dividere i colori per riga
    printf("%*s     /  /     .'  -=-'   `.      ", padding, "");
    set_color(COLOR_GOLD); printf("______  __    __   ______   __    __  ________ \n");

    set_color(COLOR_SNAKE);
    printf("%*s    /  /    .'             )    ", padding, "");
    set_color(COLOR_GOLD); printf("/      \\|  \\  |  \\ /      \\ |  \\  /  \\|        \\\n");

    set_color(COLOR_SNAKE);
    printf("%*s  _/  /   .'        _.)   /    ", padding, "");
    set_color(COLOR_GOLD); printf("|  $$$$$$\\$$$$ |  $$|  $$$$$$\\| $$ /  $$| $$$$$$$$\n");

    set_color(COLOR_SNAKE);
    printf("%*s / o   o        _.-' /  .'     ", padding, "");
    set_color(COLOR_GOLD); printf("| $$__\\$$| $$ \\| $$| $$__| $$| $$/  $$ | $$__    \n");

    set_color(COLOR_SNAKE);
    printf("%*s \\          _.-'    / .'*|      ", padding, "");
    set_color(COLOR_GOLD); printf("\\$$    \\| $$  \\ $$| $$    $$| $$  $$  | $$  \\   \n");

    set_color(COLOR_SNAKE);
    printf("%*s  \\______.-'//    .'.' \\*|      ", padding, "");
    set_color(COLOR_GOLD); printf("_\\$$$$$$\\ $$ |\\ $$| $$$$$$$$| $$$$$\\  | $$$$$  \n");

    set_color(COLOR_SNAKE);
    printf("%*s   \\|  \\ | //   .'.' _ |*|     ", padding, "");
    set_color(COLOR_GOLD); printf("|  \\__| $$ $$ | \\$$| $$  | $$| $$ \\$$\\ | $$_____ \n");

    set_color(COLOR_SNAKE);
    printf("%*s    `   \\|//  .'.'_ _ _|*|      ", padding, "");
    set_color(COLOR_GOLD); printf("\\$$    $$ $$ |  $$| $$  | $$| $$  \\$$\\| $$     \\\n");

    set_color(COLOR_SNAKE);
    printf("%*s     .  .// .'.' | _ _ \\*|       ", padding, "");
    set_color(COLOR_GOLD); printf("\\$$$$$$ \\$$    \\$$ \\$$   \\$$ \\$$   \\$$ \\$$$$$$$$\n");

    // Ultime righe del cobra (solo Snake color)
    set_color(COLOR_SNAKE);
    printf("%*s     \\`-|\\_/ /    \\ _ _ \\*\\\n", padding, "");
    printf("%*s      `/'\\__/      \\ _ _ \\*\\\n", padding, "");
    printf("%*s     /^|            \\ _ _ \\*\n", padding, "");
    printf("%*s    '  `             \\ _ _ \\ \n", padding, "");

    
    // Box opzioni con allineamento dinamico
    set_color(COLOR_WALL);
    print_menu_line("  \xC9====================================================\xBB");
    
    set_color(COLOR_WALL); printf("%*s\xBA", (get_window_width()-80)/2 + 2, ""); 
    set_color(COLOR_GOLD); printf("  [1]"); set_color(COLOR_DEFAULT); printf(" GIOCA TU (MISSIONE FUGA)             ");
    set_color(COLOR_WALL); printf("\xBA\n");

    set_color(COLOR_WALL); printf("%*s\xBA", (get_window_width()-80)/2 + 2, ""); 
    set_color(COLOR_GOLD); printf("  [2]"); set_color(COLOR_DEFAULT); printf(" DIJKSTRA AI (VERSO USCITA)           ");
    set_color(COLOR_WALL); printf("\xBA\n");

    set_color(COLOR_WALL); printf("%*s\xBA", (get_window_width()-80)/2 + 2, ""); 
    set_color(COLOR_GOLD); printf("  [3]"); set_color(COLOR_DEFAULT); printf(" A* STAR AI (FUGA OTTIMIZZATA)        ");
    set_color(COLOR_WALL); printf("\xBA\n");

    set_color(COLOR_WALL); printf("%*s\xBA", (get_window_width()-80)/2 + 2, ""); 
    set_color(COLOR_GOLD); printf("  [4]"); set_color(COLOR_DEFAULT); printf(" INFORMAZIONI SUL GIOCO               ");
    set_color(COLOR_WALL); printf("\xBA\n");

    set_color(COLOR_WALL);
    print_menu_line("  \xCC====================================================\xB9");

    printf("%*s\xBA", (get_window_width()-80)/2 + 2, ""); 
    set_color(COLOR_FOOD); printf("  [5]"); set_color(COLOR_DEFAULT); printf(" ESCI                                 ");
    set_color(COLOR_WALL); printf("\xBA\n");

    print_menu_line("  \xC8====================================================\xBC");

    // Rendering decorativo Snake inferiore
    set_color(COLOR_SNAKE);
    print_menu_line("                      __    __    __    __");
    print_menu_line("                     /  \\  /  \\  /  \\  /  \\");
    print_menu_line("____________________/  __\\/  __\\/  __\\/  __\\_____________________________");
    print_menu_line("___________________/  /__/  /__/  /__/  /________________________________");
    print_menu_line("                   | / \\   / \\   / \\   / \\  \\____");
    print_menu_line("                   |/   \\_/   \\_/   \\_/   \\    o \\");
    print_menu_line("                                           \\_____/--<");

    set_color(COLOR_GOLD);
    print_menu_line("            >> SELEZIONA OPZIONE: ");

    // Loop di ascolto input menu
    while (1) {
        if (_kbhit()) {
            char c = _getch();
            Beep(600, 50); // Feedback sonoro selezione
            if (c == '1') { is_ai = 0; ai_mode = 0; return 1; }
            if (c == '2') { is_ai = 1; ai_mode = 1; return 1; }
            if (c == '3') { is_ai = 1; ai_mode = 2; return 1; }
            if (c == '4') { show_info(); return 0; }
            if (c == '5') exit(0);
        }
    }
}

// Disegna la cornice del campo di gioco basata sulle costanti WIDTH/HEIGHT
void draw_border() {
    system("cls"); 
    draw_ui();
    set_color(COLOR_WALL);
    
    // Angolo sup. sx e linea superiore
    gotoxy(0, 1); printf("%c", 201);
    for (int i = 1; i < WIDTH; i++) printf("%c", 205);
    printf("%c", 187); // Angolo sup. dx
    
    // Pareti laterali
    for (int i = 2; i <= HEIGHT; i++) {
        gotoxy(0, i); printf("%c", 186);
        gotoxy(WIDTH, i); printf("%c", 186);
    }
    
    // Angolo inf. sx e linea inferiore
    gotoxy(0, HEIGHT + 1); printf("%c", 200);
    for (int i = 1; i < WIDTH; i++) printf("%c", 205);
    printf("%c", 188); // Angolo inf. dx
}
/* --- LOGICA DI GIOCO E AGGIORNAMENTO FRAME --- */

void logic() {
    // 1. GESTIONE SCIA: Cancella visivamente l'ultimo segmento della coda
    Point tail = snake[length - 1];
    if (tail.x >= 0) {
        gotoxy(tail.x, tail.y);
        printf(" ");
    }

    // 2. ACQUISIZIONE INPUT / CALCOLO TRAIETTORIA AI
    if (is_ai) {
        if (_kbhit() && toupper(_getch()) == 'Q') { lives = 0; return; }
        direction = get_ai_direction();
    } else {
        if (_kbhit()) {
            int k = _getch();
            if (k == 224) k = _getch(); // Gestione tasti estesi (frecce)

            // Mapping WASD su codici direzionali standard
            int lower_k = tolower(k);
            if (lower_k == 'w') k = 72;
            else if (lower_k == 's') k = 80;
            else if (lower_k == 'a') k = 75;
            else if (lower_k == 'd') k = 77;

            // Filtro anti-inversione a 180 gradi (impedisce il suicidio istantaneo)
            if ((k == 72 && direction != 80) || (k == 80 && direction != 72) ||
                (k == 75 && direction != 77) || (k == 77 && direction != 75)) {
                direction = k;
            }
        }
    }

    // 3. AGGIORNAMENTO VETTORE POSIZIONI (Shift dei segmenti)
    for (int i = length - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }

    // Traslazione della testa in base alla direzione corrente
    if (direction == 72)      snake[0].y--; // SU
    else if (direction == 80) snake[0].y++; // GIU
    else if (direction == 75) snake[0].x--; // SINISTRA
    else if (direction == 77) snake[0].x++; // DESTRA

    // 4. TRIGGER EVENTO FINALE: Raggiungimento soglia 500 punti
    if (score >= 500 && !door_open) {
        door_open = 1;
        Beep(800, 150); Beep(1200, 200); // Feedback acustico sblocco
        
        // Rimozione fisica dell'ostacolo sulla griglia alle coordinate del varco
        set_color(COLOR_DEFAULT);
        gotoxy(exit_door.x, exit_door.y);
        printf(" "); 
    }

    // 5. ANIMAZIONE UI: Toggle stato per effetto lampeggiante
    if (door_open) {
        flash_state = !flash_state;
        draw_ui(); 
    }

    // 6. VERIFICA CONDIZIONE DI VITTORIA: Collisione con il varco d'uscita
    if (door_open && snake[0].x == exit_door.x && snake[0].y == exit_door.y) {
        system("cls");
        set_color(COLOR_GOLD);
        printf("\n\n   ==============================================\n");
        printf("      MISSIONE COMPIUTA! SEI USCITO DALLA GRIGLIA! \n");
        printf("      PUNTEGGIO FINALE: %d                        \n", score);
        printf("   ==============================================\n");
        set_color(COLOR_DEFAULT);
        printf("\n   Premi un tasto per tornare al menu...");
        _getch();
        lives = 0; 
        return;
    }

    // 7. GESTIONE ALIMENTAZIONE: Collisione cibo standard ($)
    if (snake[0].x == food.x && snake[0].y == food.y) {
        Beep(1200, 10);
        score += 10;
        length++;
        draw_ui();
        food.x = rand() % (WIDTH - 2) + 1;
        food.y = rand() % (HEIGHT - 1) + 2;
        
        // Spawn probabilistico (20%) di oggetti speciali
        if (rand() % 5 == 0) {
            special_type = (rand() % 2) + 1; // 1: ORO (G), 2: VELENO (P)
            special.x = rand() % (WIDTH - 2) + 1;
            special.y = rand() % (HEIGHT - 1) + 2;
        }
    }

    // 8. GESTIONE BONUS/MALUS: Collisione oggetti speciali
    if (special_type && snake[0].x == special.x && snake[0].y == special.y) {
        if (special_type == 1) { // Bonus Oro: incrementa score e massa
            score += 50;
            length += 3;
        } else { // Malus Veleno: detrazione punti
            score = (score >= 30) ? score - 30 : 0;
        }
        special_type = 0;
        draw_ui();
        gotoxy(special.x, special.y); printf(" "); 
    }

    // 9. RENDERING FRAME: Disegno dei target e del corpo del serpente
    set_color(COLOR_FOOD);
    gotoxy(food.x, food.y); printf("$");

    if (special_type == 1) {
        set_color(COLOR_GOLD);
        gotoxy(special.x, special.y); printf("G");
    } else if (special_type == 2) {
        set_color(COLOR_POISON);
        gotoxy(special.x, special.y); printf("P");
    }

    set_color(is_ai ? COLOR_AI : COLOR_SNAKE);
    for (int i = 0; i < length; i++) {
        if (snake[i].x < 0) continue;
        // Evita di ridisegnare sopra il varco se aperto per non ostruirlo visivamente
        if (door_open && snake[i].x == exit_door.x && snake[i].y == exit_door.y) continue;

        gotoxy(snake[i].x, snake[i].y);
        printf(i == 0 ? "O" : "o");
    }
}

// Inizializza le variabili di stato per un nuovo round di gioco
void setup_round() {
    length = 5;
    direction = 77; // Start verso DESTRA
    special_type = 0;
    snake[0].x = WIDTH / 2;
    snake[0].y = HEIGHT / 2;
    // Reset posizioni array corpo (coordinate negative = inattivo)
    for(int i = 1; i < 1000; i++) { snake[i].x = -1; snake[i].y = -1; }
    food.x = rand() % (WIDTH - 2) + 1;
    food.y = rand() % (HEIGHT - 1) + 2; 
}

/* --- ENTRY POINT E CICLO PRINCIPALE --- */

int main() {
    SetConsoleOutputCP(437); // Imposta codifica OEM-US per caratteri ASCII estesi
    srand((unsigned)time(NULL));
    hide_cursor();

    // Loop infinito del programma (ritorno costante al menu principale)
    while(1) {
        update_offset(); // Allineamento basato sulla risoluzione terminale
        int choice = show_menu();

        if (choice == 0) continue; // Gestione rientro da pagina INFO

        if (choice == 1) {
            // Inizializzazione nuova sessione di gioco
            lives = is_ai ? 1 : 3; 
            score = 0; 
            door_open = 0;

            while(lives > 0) {
                setup_round(); 
                draw_border();
                
                // Ciclo di esecuzione frame
                while(1) {
                    logic();
                    if(!lives) break; 
                    
                    Sleep(is_ai ? 15 : 80); // Frequenza di campionamento (più alta per AI)
                    
                    // GESTIONE COLLISIONI CRITICHE
                    int hit = 0;
                    for(int i=1; i<length; i++) {
                        if(snake[0].x == snake[i].x && snake[0].y == snake[i].y) hit = 1;
                    }
                    
                    // Verifica impatto bordi (con eccezione per il varco di uscita)
                    int wall_hit = (snake[0].x <= 0 || snake[0].x >= WIDTH || snake[0].y <= 1 || snake[0].y >= HEIGHT + 1);
                    if (door_open && snake[0].x == exit_door.x && snake[0].y == exit_door.y) wall_hit = 0;

                    if (wall_hit || hit) {
                        lives--; 
                        // Feedback sonoro morte (frequenze discendenti)
                        for(int f = 800; f > 200; f -= 100) Beep(f, 50);

                        if(lives > 0) { 
                            set_color(COLOR_FOOD); 
                            gotoxy(WIDTH/2-5, HEIGHT/2); 
                            printf(" VITA PERSA "); 
                            _getch(); 
                            break; // Reset round con sottrazione vita
                        } else { 
                            set_color(COLOR_FOOD); 
                            gotoxy(WIDTH/2-5, HEIGHT/2); 
                            printf(" GAME OVER "); 
                            _getch(); 
                            goto game_end; // Fine sessione, ritorno al menu
                        }
                    }
                }
            }
            game_end:; 
        }
    }
    return 0;
}