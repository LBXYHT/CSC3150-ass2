#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <curses.h>
#include <termios.h>
#include <fcntl.h>

#define ROW 10
#define COLUMN 50 
#define length_log 15

struct Node{
	int x , y; 
	Node( int _x , int _y ) : x( _x ) , y( _y ) {}; 
	Node(){} ; 
} frog ;

bool status = true;
int game_status = 0;

char map[ROW+10][COLUMN] ; 
pthread_mutex_t mutex;
pthread_mutex_t log_mutex;
pthread_cond_t gameover;


void moveleft(int front, int rear, int i) {
	if (front < rear) {
		//continuing log
		if (i <= front || i > rear) {
			putchar('=');
		} else {
			putchar(' ');
		}
	} else {
		//divided log
		if (i <= front && i > rear) {
			putchar('=');
		} else {
			putchar(' ');
		}
	}
}

void moveright(int front, int rear, int i) {
	if (front > rear) {
		//divided log
		if (i < rear || i >= front) {
			putchar('=');
		} else {
			putchar(' ');
		}
	} else {
		//continuing log
		if (i >= front && i < rear) {
			putchar('=');
		} else {
			putchar(' ');
		}
	}
}

// Determine a keyboard is hit or not. If yes, return 1. If not, return 0. 
int kbhit(void){
	struct termios oldt, newt;
	int ch;
	int oldf;

	tcgetattr(STDIN_FILENO, &oldt);

	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);

	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);

	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

	ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if(ch != EOF)
	{
		ungetc(ch, stdin);
		return 1;
	}
	return 0;
}

void *logs_move( void *t ){
	long log_index;
	log_index = (long) t; //index of row
	int front, rear, front_left, rear_left; //the front and rear (pointers) of logs

	front = rand() % COLUMN;
	rear = front + length_log - COLUMN;

	front_left = COLUMN-front;
	rear_left = COLUMN-rear;
	//set right direction as positive, each left direction move in a opposite direction and front_left/rear_left minus as front/rear add

	/*  Move the logs  */
	while (status != false) {

		pthread_mutex_lock(&log_mutex); //lock each row for moving independently

		printf("\033[%dH\033[K", log_index + 1);
		for (int i = 0; i < COLUMN; i++) {
			//row = 2,4,6,8
			if (log_index == 2 || log_index == 4 || log_index == 6 || log_index == 8) {
				moveright(front, rear, i);
			//row = 1,3,5,7,9
			} else if (log_index == 1 || log_index == 3 || log_index == 5 || log_index == 7 || log_index == 9) {
				moveleft(front_left, rear_left, i);
			}
		}
		
		pthread_mutex_unlock(&log_mutex);

		//let frog move with logs
		if (frog.x == log_index) {
			if (log_index == 1 || log_index == 3 || log_index == 5 || log_index == 7 || log_index == 9) {
				frog.y--;
			} else if (log_index == 2 || log_index == 4 || log_index == 6 || log_index == 8) {
				frog.y++;
			}
		}

		usleep(100000);

		//check if logs are out of range
		front++;
		front_left--;
		if (front >= COLUMN) {
			front = front % COLUMN;
		}
		if (front_left <= 0) {
			front_left += COLUMN;
		}

		rear++;
		rear_left--;
		if (rear >= COLUMN) {
			rear = rear % COLUMN;
		}
		if (rear_left <= 0) {
			rear_left += COLUMN;
		}

		/*  Check keyboard hits, to change frog's position or quit the game. */
		pthread_mutex_lock(&mutex);
		char ch;
		if (kbhit()) {
			ch = getchar();
			if ((ch == 'w' || ch == 'W') && frog.x > 0) {
				frog.x--;
			} else if ((ch == 's' || ch == 'S') && frog.x < ROW) {
				frog.x++;
			} else if ((ch == 'a' || ch == 'A') && frog.y >= 0) {
				frog.y--;
			} else if ((ch == 'd' || ch == 'D') && frog.y <= COLUMN) {
				frog.y++;
			} else if (ch == 'q' || ch == 'Q') {
				status = false;
				pthread_cond_signal(&gameover);
			}
		}

		pthread_mutex_lock(&log_mutex);

		printf("\033[%dH", ROW+1);
		for (int i = 0; i < COLUMN; i++) {
			putchar('|');
		}
		printf("\033[%d;%dH", frog.x + 1, frog.y);
		putchar('0');

		//check if frog is out of the left/right side
		if (frog.y < 0 || frog.y > COLUMN) {
			status = false;
			game_status = -1;
			pthread_cond_signal(&gameover);
		}
		
		//check if frog falls into river
		if (frog.x == log_index) {
			if ((log_index == 1 || log_index == 3 || log_index == 5 || log_index == 7 || log_index == 9) && (front > rear)) {
				//moving left, divided log
				if (frog.y < front_left + 2 && frog.y >= rear_left + 2) {
					status = false;
					game_status = -1;
					pthread_cond_signal(&gameover);
				}
			} else if ((log_index == 1 || log_index == 3 || log_index == 5 || log_index == 7 || log_index == 9) && (front < rear)) {
				//moving left, continuing log
				if (frog.y >= front_left + 3 || frog.y < rear_left + 3) {
					status = false;
					game_status = -1;
					pthread_cond_signal(&gameover);
				}
			} else if ((log_index == 2 || log_index == 4 || log_index == 6 || log_index == 8) && (front > rear)) {
				//moving right, continuing log
				if (frog.y <= rear && frog.y > front) {
					status = false;
					game_status = -1;
					pthread_cond_signal(&gameover);
				}
			} else if ((log_index == 2 || log_index == 4 || log_index == 6 || log_index == 8) && (front < rear)) {
				//moving right, divided log
				if (frog.y < front || frog.y >= rear) {
					status = false;
					game_status = -1;
					pthread_cond_signal(&gameover);
				}
			}
		}

		//check if frog arrives at another side of river
		if (frog.x == 0) {
			status = false;
			game_status = 1;
			pthread_cond_signal(&gameover);
		}	

		pthread_mutex_unlock(&log_mutex);

		pthread_mutex_unlock(&mutex);
	}
	/*  Print the map on the screen  */
}

int main( int argc, char *argv[] ){
	pthread_t thread[ROW];
	int rc;
	//initialize mutex and condition
	pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&log_mutex, NULL);
	pthread_cond_init(&gameover, NULL);

	printf("\033[2J");
	// Initialize the river map and frog's starting position
	memset( map , 0, sizeof( map ) ) ;
	int i , j ; 
	for( i = 1; i < ROW; ++i ){	
		for( j = 0; j < COLUMN - 1; ++j )	
			map[i][j] = ' ' ;  
	}	

	for( j = 0; j < COLUMN - 1; ++j )	
		map[ROW][j] = map[0][j] = '|' ;

	for( j = 0; j < COLUMN - 1; ++j )	
		map[0][j] = map[0][j] = '|' ;

	frog = Node( ROW, (COLUMN-1) / 2 ) ; 
	map[frog.x][frog.y] = '0' ; 

	//Print the map into screen
	for( i = 0; i <= ROW; ++i)	
		puts( map[i] );

	/*  Create pthreads for wood move and frog control.  */
	for (int i = 1; i < ROW; i++) {
		rc = pthread_create(&thread[i], NULL, logs_move, (void*) i);
		if (rc) {
			printf("ERROR: return code from pthread_create() is %d", rc);
			exit(1);
		}
	}
	
	pthread_cond_wait(&gameover, &mutex); //wait for the gameover signal

	/*  Display the output for user: win, lose or quit.  */
	printf("\033[H\033[2J");
	if (game_status == -1) {
		printf("You lose the game!!\n");
	} else if (game_status == 1) {
		printf("You win the game!!\n");
	} else {
		printf("You exit the game.\n");
	}

	return 0;
}
