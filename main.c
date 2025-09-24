#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

#define MAX_LINES 1000
#define MAX_LENGTH 256
#define DATE_LENGTH 11

typedef struct {
    char text[MAX_LENGTH];
    char deadline[DATE_LENGTH];
    int completed;
} TodoItem;

TodoItem items[MAX_LINES];
int line_count = 0;
int selected_index = 0;
int scroll_offset = 0;
int screen_rows = 0;
int screen_cols = 0;
int edit_mode = 0;
int time_mode = 0;
int cursor_pos = 0;
int date_part = 0;
char filename[256] = "todo.txt";
int auto_save = 1;

int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int get_days_in_month(int month, int year) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days[month - 1];
}

void get_current_date(char *date_str) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(date_str, DATE_LENGTH, "%d.%m.%Y", tm_info);
}

int parse_date(const char *date_str, int *day, int *month, int *year) {
    return sscanf(date_str, "%2d.%2d.%4d", day, month, year) == 3;
}

void format_date(char *date_str, int day, int month, int year) {
    snprintf(date_str, DATE_LENGTH, "%02d.%02d.%04d", day, month, year);
}

void increment_date_part(char *date_str, int part, int increment) {
    int day, month, year;
    if (!parse_date(date_str, &day, &month, &year)) {
        get_current_date(date_str);
        return;
    }
    
    switch (part) {
        case 0:
            day += increment;
            while (day > get_days_in_month(month, year)) day = 1;
            while (day < 1) day = get_days_in_month(month, year);
            break;
        case 1:
            month += increment;
            while (month > 12) month = 1;
            while (month < 1) month = 12;
            if (day > get_days_in_month(month, year)) {
                day = get_days_in_month(month, year);
            }
            break;
        case 2:
            year += increment;
            if (year < 1900) year = 1900;
            if (year > 2100) year = 2100;
            if (day > get_days_in_month(month, year)) {
                day = get_days_in_month(month, year);
            }
            break;
    }
    
    format_date(date_str, day, month, year);
}

void get_terminal_size() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    screen_cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    screen_rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    screen_rows = w.ws_row;
    screen_cols = w.ws_col;
#endif
}

void clear_screen() {
    system("cls || clear");
}

void hide_cursor() {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
#else
    printf("\033[?25l");
#endif
}

void show_cursor() {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
#else
    printf("\033[?25h");
#endif
}

void delay(int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

int read_todo_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return 0;
    }
    
    line_count = 0;
    char line[MAX_LENGTH + DATE_LENGTH + 10];
    
    while (fgets(line, sizeof(line), file) != NULL && line_count < MAX_LINES) {
        line[strcspn(line, "\n")] = '\0';
        
        char *separator = strstr(line, " | ");
        if (separator) {
            *separator = '\0';
            char *text = line;
            if (strncmp(text, "D ", 2) == 0) {
                items[line_count].completed = 1;
                strncpy(items[line_count].text, text + 2, MAX_LENGTH - 1);
            } else {
                items[line_count].completed = 0;
                strncpy(items[line_count].text, text, MAX_LENGTH - 1);
            }
            strncpy(items[line_count].deadline, separator + 3, DATE_LENGTH - 1);
        } else {
            if (strncmp(line, "D ", 2) == 0) {
                items[line_count].completed = 1;
                strncpy(items[line_count].text, line + 2, MAX_LENGTH - 1);
            } else {
                items[line_count].completed = 0;
                strncpy(items[line_count].text, line, MAX_LENGTH - 1);
            }
            get_current_date(items[line_count].deadline);
        }
        
        line_count++;
    }
    
    fclose(file);
    return 1;
}

void save_todo_file(const char *filename) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        return;
    }
    
    for (int i = 0; i < line_count; i++) {
        if (items[i].completed) {
            fprintf(file, "D %s | %s\n", items[i].text, items[i].deadline);
        } else {
            fprintf(file, "%s | %s\n", items[i].text, items[i].deadline);
        }
    }
    
    fclose(file);
}

void insert_line(int index, const char *text) {
    if (line_count >= MAX_LINES) return;
    
    for (int i = line_count; i > index; i--) {
        items[i] = items[i-1];
    }
    
    strncpy(items[index].text, text, MAX_LENGTH - 1);
    get_current_date(items[index].deadline);
    items[index].completed = 0;
    line_count++;
}

void delete_line(int index) {
    if (line_count <= 0 || index >= line_count) return;
    
    for (int i = index; i < line_count - 1; i++) {
        items[i] = items[i+1];
    }
    
    line_count--;
}

void move_completed_to_bottom(int index) {
    if (index >= line_count - 1) return;
    
    TodoItem temp = items[index];
    for (int i = index; i < line_count - 1; i++) {
        items[i] = items[i + 1];
    }
    items[line_count - 1] = temp;
}

void display_status_bar() {
    printf("\033[7m");
    
    char status[256];
    if (time_mode) {
        const char *parts[] = {"day", "month", "year"};
        snprintf(status, sizeof(status), " TIME - %s | Edit: %s | Auto-save: %s", filename, parts[date_part], auto_save ? "ON" : "OFF");
    } else if (edit_mode) {
        snprintf(status, sizeof(status), " EDIT - %s | Line %d/%d | Auto-save: %s", filename, selected_index + 1, line_count, auto_save ? "ON" : "OFF");
    } else {
        snprintf(status, sizeof(status), " NORMAL - %s | Line %d/%d | Auto-save: %s", filename, selected_index + 1, line_count, auto_save ? "ON" : "OFF");
    }
    
    int padding = screen_cols - strlen(status);
    printf("%s%*s", status, padding, "");
    
    printf("\033[0m\n");
}

void display_lines() {
    clear_screen();
    get_terminal_size();
    
    if (selected_index < scroll_offset) {
        scroll_offset = selected_index;
    } else if (selected_index >= scroll_offset + screen_rows - 2) {
        scroll_offset = selected_index - screen_rows + 2;
    }
    
    int visible_lines = screen_rows - 2;
    int text_width = screen_cols - DATE_LENGTH - 15;

    for (int i = 0; i < visible_lines; i++) {
        int line_idx = scroll_offset + i;
        
        if (line_idx >= line_count) {
            printf("~\n");
            continue;
        }
        
        if (line_idx == selected_index) {
            printf("\033[7m");
        }
        
        if (items[line_idx].completed) {
            printf("D    ");
        } else {
            printf("     ");
        }
        
        if (edit_mode && line_idx == selected_index && !time_mode) {
            printf("%.*s", cursor_pos, items[line_idx].text);
            printf("|");
            printf("%s", items[line_idx].text + cursor_pos);
            
            int text_len = strlen(items[line_idx].text);
            int spaces = text_width - text_len - 1;
            if (spaces > 0) printf("%*s", spaces, "");
        } else {
            printf("%s", items[line_idx].text);
            
            int text_len = strlen(items[line_idx].text);
            int spaces = text_width - text_len;
            if (spaces > 0) printf("%*s", spaces, "");
        }
        
        printf(" [");
        
        if (time_mode && line_idx == selected_index) {
            int day, month, year;
            parse_date(items[line_idx].deadline, &day, &month, &year);
            
            switch (date_part) {
                case 0:
                    printf("\033[7m%02d\033[0m.%02d.%04d", day, month, year);
                    break;
                case 1:
                    printf("%02d.\033[7m%02d\033[0m.%04d", day, month, year);
                    break;
                case 2:
                    printf("%02d.%02d.\033[7m%04d\033[0m", day, month, year);
                    break;
            }
        } else {
            printf("%s", items[line_idx].deadline);
        }
        
        printf("]");
        
        if (line_idx == selected_index) {
            printf("\033[0m");
        }
        
        printf("\n");
    }
    
    display_status_bar();
}

#ifdef _WIN32
int get_key() {
    int ch = _getch();
    if (ch == 0 || ch == 224) {
        ch = _getch();
        switch (ch) {
            case 72: return 0x100 + 'U';
            case 75: return 0x100 + 'L';
            case 77: return 0x100 + 'R';
            case 80: return 0x100 + 'D';
            case 83: return 0x100 + 'X';
        }
    }
    return ch;
}
#else
int get_key() {
    struct termios oldt, newt;
    int ch;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    ch = getchar();
    
    if (ch == 27) {
        ch = getchar();
        if (ch == 91) {
            ch = getchar();
            switch (ch) {
                case 65: ch = 0x100 + 'U'; break;
                case 66: ch = 0x100 + 'D'; break;
                case 67: ch = 0x100 + 'R'; break;
                case 68: ch = 0x100 + 'L'; break;
                case 51: 
                    if (getchar() == 126) ch = 0x100 + 'X';
                    break;
            }
        }
    }
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif

void handle_normal_mode(int key) {
    switch (key) {
        case 0x100 + 'U': 
            if (selected_index > 0) selected_index--;
            break;
        case 0x100 + 'D': 
            if (selected_index < line_count - 1) selected_index++;
            break;
        case 'd':  
            items[selected_index].completed = 1;
            move_completed_to_bottom(selected_index);
            if (selected_index >= line_count - 1) selected_index = line_count - 1;
            if (auto_save) save_todo_file(filename);
            break;
        case 'i':
            edit_mode = 1;
            time_mode = 0;
            cursor_pos = strlen(items[selected_index].text);
            show_cursor();
            break;
        case 't': 
            time_mode = 1;
            edit_mode = 0;
            date_part = 0;
            hide_cursor();
            break;
        case 'o': 
            insert_line(selected_index + 1, "");
            selected_index++;
            edit_mode = 1;
            time_mode = 0;
            cursor_pos = 0;
            show_cursor();
            if (auto_save) save_todo_file(filename);
            break;
        case 'O':  
            insert_line(selected_index, "");
            edit_mode = 1;
            time_mode = 0;
            cursor_pos = 0;
            show_cursor();
            if (auto_save) save_todo_file(filename);
            break;
        case 'x': 
            if (line_count > 0) {
                delete_line(selected_index);
                if (selected_index >= line_count) selected_index = line_count - 1;
                if (auto_save) save_todo_file(filename);
            }
            break;
        case 's':  
            save_todo_file(filename);
            printf("\nFile saved!\n");
            delay(1000);
            break;
        case 'a':  
            auto_save = !auto_save;
            break;
        case '+':
        case '=': 
            if (screen_cols < 200) screen_cols += 10;
            break;
        case '-':
        case '_':  
            if (screen_cols > 80) screen_cols -= 10;
            break;
        case 'q': 
            exit(0);
            break;
    }
}

void handle_edit_mode(int key) {
    switch (key) {
        case 27:  
            edit_mode = 0;
            time_mode = 0;
            hide_cursor();
            if (auto_save) save_todo_file(filename);
            break;
        case '\r': case '\n':  
            insert_line(selected_index + 1, items[selected_index].text + cursor_pos);
            items[selected_index].text[cursor_pos] = '\0';
            selected_index++;
            cursor_pos = 0;
            if (auto_save) save_todo_file(filename);
            break;
        case 0x100 + 'R':  
            if (cursor_pos < strlen(items[selected_index].text)) cursor_pos++;
            break;
        case 0x100 + 'L': 
            if (cursor_pos > 0) cursor_pos--;
            break;
        case 0x100 + 'U':
            if (selected_index > 0) {
                selected_index--;
                cursor_pos = strlen(items[selected_index].text);
            }
            break;
        case 0x100 + 'D': 
            if (selected_index < line_count - 1) {
                selected_index++;
                cursor_pos = strlen(items[selected_index].text);
            }
            break;
        case 127:
        case 0x100 + 'X':
            if (cursor_pos > 0) {
                memmove(items[selected_index].text + cursor_pos - 1, 
                        items[selected_index].text + cursor_pos, 
                        strlen(items[selected_index].text) - cursor_pos + 1);
                cursor_pos--;
                if (auto_save) save_todo_file(filename);
            } else if (selected_index > 0) {
                int prev_len = strlen(items[selected_index - 1].text);
                strcat(items[selected_index - 1].text, items[selected_index].text);
                delete_line(selected_index);
                selected_index--;
                cursor_pos = prev_len;
                if (auto_save) save_todo_file(filename);
            }
            break;
        default:
            if (isprint(key)) {
                if (strlen(items[selected_index].text) < MAX_LENGTH - 1) {
                    memmove(items[selected_index].text + cursor_pos + 1, 
                            items[selected_index].text + cursor_pos, 
                            strlen(items[selected_index].text) - cursor_pos + 1);
                    items[selected_index].text[cursor_pos] = key;
                    cursor_pos++;
                    if (auto_save) save_todo_file(filename);
                }
            }
            break;
    }
}

void handle_time_mode(int key) {
    switch (key) {
        case 27:  
            time_mode = 0;
            if (auto_save) save_todo_file(filename);
            break;
        case 0x100 + 'R': 
            date_part = (date_part + 1) % 3;
            break;
        case 0x100 + 'L': 
            date_part = (date_part - 1 + 3) % 3;
            break;
        case 0x100 + 'U': 
            increment_date_part(items[selected_index].deadline, date_part, 1);
            if (auto_save) save_todo_file(filename);
            break;
        case 0x100 + 'D':
            increment_date_part(items[selected_index].deadline, date_part, -1);
            if (auto_save) save_todo_file(filename);
            break;
        case '\r': case '\n': 
            time_mode = 0;
            if (auto_save) save_todo_file(filename);
            break;
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        strncpy(filename, argv[1], sizeof(filename) - 1);
    }
    
    if (!read_todo_file(filename)) {
        line_count = 1;
        strcpy(items[0].text, "New todo list");
        get_current_date(items[0].deadline);
        items[0].completed = 0;
    }
    
    hide_cursor();
    get_terminal_size();
    
    while (1) {
        display_lines();
        int key = get_key();
        
        if (time_mode) {
            handle_time_mode(key);
        } else if (edit_mode) {
            handle_edit_mode(key);
        } else {
            handle_normal_mode(key);
        }
    }
    
    show_cursor();
    return 0;
}