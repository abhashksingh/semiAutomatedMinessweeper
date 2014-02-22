#include <iostream>
#include <iomanip>
#include <stdlib.h> // for atoi  only
#include <sstream>
#include <time.h>
#include <math.h>
#include <strstream>
using namespace std;
bool autoA; // to make testing easier, not really part of the game
bool autoB; // automaticly follow hints
bool autoC;
int gmines; // number of mines in field
int gcells; // number of cells left to open
// Type Status : status of a cell
// closed : initial state, the contents of this cell are unknown
// open : user has opened this cell
// marked : user has marked this cell as containing a mine. The analyzer only uses
// this state when looking for mines.
//invalid: cell is invalid and not part of the playfield. We place a 1 cell border of invalid
// cells around the playfield to safe on a number of array bound checks.
typedef enum {closed,opened,marked,invalid} Status;
typedef enum {opening,marking} Mode;
typedef enum {heuristicField,rulesField} Hintfield;
// Type Hint : Hints for a cell
// Hints are given according to certain rules. Details of rules can be found in the
// report. Overview :
// ruleA : X neighbour mines, X found, so rest is safe
// ruleB : X neighbour mines, Y found and X-Y unknown, so unknowns contain mines
// ruleC : All unknown neighbours of cell A are a subset of the unknown neighbours of cell B,
// outside subset is safe
typedef enum {nohint,ruleA,ruleB,rulec,ruleC,ruleD} Hint;
typedef enum {undetermined,sub,super,both} SetType;
// Class play_cell : a single cell of the playfield.
// This class contains all info relating to a single cell, including both game info and
// analysis data.
class play_cell {

public:
	bool mine; // if true, contains actual mine
	Status status; // What has the user done with this cell
	int neighbour_mines; // number of mines in direct neighbour cells

	// analysis info
	// Note that this info is seen by a human player in 'one' look (if she pays attention
	// that is ':-)' and can also be found on the fly by checking all neighbours each time.

	int neighbour_mines_found; // number of neighbour mines found
	int neighbours_closed; // number of neighbour cells not opened or marked by user but that can be opened or marked
	Hint hint; // hint given by program to user

	play_cell* set_membership[8];// the sets that this *closed* cell is a element of ie, which open cells have this closed cell as neighbour.It is meaningless when cell is not closed
	int membership_cntr; // number of elements in set_memberships,only valid for closed cells
	SetType unc; // is the set of closed neighbours (UNC) of this *open* cell a subset,superset,or both of that of other open cells ?
	play_cell* super_sets[8]; // The sets which are a superset of the UNC of this *open* cell
	play_cell* sub_sets[21]; // The sets which are a subset of the UNC of this *open* cell
	int sub_set_cntr; // counts subsets

	int heur_numerator[8]; // numerators of all chances a mine is not in this closed cell
	int heur_denominator[8]; // denominator of all chances a mine is not in this closed cell
	int rating; // rating of this cell by the heuristic function

	// implementation info
	int row; // one row and colomn, used for accessing neighbours
	int colomn;
	//the constructor
	play_cell() {
		mine=false;
		status = closed;
		neighbour_mines=0;

	//analysis initialisation
		neighbour_mines_found=0;
		neighbours_closed=8; // init on eight,if cell is adjacent to a border, the create board function will decrease this accordingly
		hint=nohint;
		rating=9; // unknown yet, start with highest rating
	
		int i;
		for (i=0; i<=7; i++) {
			set_membership[i]=NULL;
			super_sets[i]=NULL;
			sub_sets[i]=NULL; // initialize first 8
			heur_numerator[i]=1;
			heur_denominator[i]=1;
		}
		for (i=8; i<=20; i++){
			sub_sets[i]=NULL; // initialize rest
		}
		membership_cntr=0;
		sub_set_cntr=0;
		unc=undetermined;
	} // constructor
};

typedef play_cell** playboard;

void OpenCell(playboard board,int row,int colomn);
void MarkCell(playboard board,int row,int colomn);
void CheckSet(playboard board,int row,int colomn);
// Neighbour
// Helper function to walk through every neighbour of a given cell
//
// 1 0 7
// 2 * 6
// 3 4 5
//
play_cell* Neighbour(playboard board,int row,int colomn,int nr) {
	switch (nr) {
		case 0:
			return &board[row-1][colomn];
		case 1:
			return &board[row-1][colomn-1];
		case 2:
			return &board[row][colomn-1];
		case 3:
			return &board[row+1][colomn-1];
		case 4:
			return &board[row+1][colomn];
		case 5:
			return &board[row+1][colomn+1];
		case 6:
			return &board[row][colomn+1];
		case 7:
			return &board[row-1][colomn+1];
	} // switch
	return NULL; // if wrong parameter supplied
}
//AddSetType
//Small helper function to add a set type to a platcell
inline void AddSetType(play_cell* cell,SetType type) {
	if (cell->unc != undetermined && cell->unc != type) // something else already ?
		cell->unc = both;
	else
		cell->unc = type;
}
//AddSet
//Small helper function to add sets of a specific kind to a cell of the opposite kind.
//Ie, when a subset set is found, it will mark the corresponding cell 'super' and add
//itself to that super's sub_set array provided it doesn't already exist
inline void AddSet(play_cell** array,play_cell* key, int max) {
	bool found=false;
	int index=-1;

	for (int n=0; n<=max; n++) {
		if (array[n] == key)
			found=true;
		if (array[n] == NULL)
			index=n;
	}

	if (found == false && index != -1) // should never be -1, arrays large enough
		array[index] = key;
	
	return;
}

#define NEIGHBOUR(nr) Neighbour(board,row,colomn,nr)
// Sort of clears the console screen
void ClearScreen() {
	for (int i=0; i<40; i++)
	cout << endl;
}

void info() {

	cout << "Project of" << endl;
	cout << "Artificial Intelligence" << endl;
	cout << "IIT Rajasthan" << endl;
	cout << "Abhash Kumar Singh(J08001)&& Arpit Toshniwaal" << endl << endl;
	cout << "***Minesweeper***" << endl << endl;
	cout << "This programs is a version of the well known minesweeper." << endl;
	cout << "It's augmented with hints regarding the presence or absence of mines." << endl;
	cout << "These hints however are generated according to rules which only take" << endl;
	cout << "the number of mines into consideration as displayed by open cells" << endl;
	cout << "and the mines AS PLACED BY THE USER.In other words, the hint" << endl;
	cout << "generator knows just as much as the player. Marking a cell wrongly" << endl;
	cout << "as mine will generate wrong hints !" << endl << endl;
	cout << "Enjoy !" << endl << endl;
	ClearScreen();
}
void help() {
	ClearScreen();
	cout << "Help of the commands which can be invoked" << endl << endl;
	cout << "? - Shows this help screen." << endl;
	cout << "a - Automaticly let's computer open all cells marked as safe" << endl;
	cout << " according to rule A." << endl;
	cout << "b - Automaticly let's computer mark all cells that"<< endl;
	cout << " contain mines according to rule B as mine." << endl;
	cout << "c - Automaticly let's computer open all cells marked as safe"<< endl;
	cout << " according to rule C." << endl;
	cout << "h - Hint switcher. Switches the hints display between rules" << endl;
	cout << " and heuristic" << endl;
	cout << "m - Marks a closed cell as containing a mine. The cell may or" << endl;
	cout << " may not actually contain a mine. If a cell already has a" << endl;
	cout << " the mark is removed." << endl;
	cout << "o - Opens a cell. If the cell contains a mine the game is over" << endl;
	cout << " if not, the cell is definitely safe and can never be" << endl;
	cout << " marked as mine again." << endl;
	cout << "q - Quits the game." << endl;
	cout << "s - Shows all. Prints a board revealing the positions of all" << endl;
	cout << " the mines. This may spoil the game ofcourse." << endl;
	cout << "r - Rules. Displays explanation of the rules used by the hint" << endl;
	cout << " generator." << endl << endl;
	//ClearScreen();
}
void rules() {

ClearScreen();
	cout << "Hints are coded in the following way :" << endl << endl;
	cout << "rule A : X neighbour mines, X found, so rest is safe (marked A)" << endl << endl;
	cout << "rule B : X neighbour mines, Y found and X-Y unknown neighbors," << endl;
	cout << " so all those neighbors (marked B) contain mines" << endl << endl;
	cout << "rule C : All unknown neighbours of cell X are a subset of the" << endl;
	cout << " unknown neighbours of cell Y, so cells outside this" << endl;
	cout << " subset are safe. (marked C when seen from superset" << endl;
	cout << " or c when seen from subset)" << endl << endl;
	cout << "rule D : Both rule A and rule C apply to this cell, so this" << endl;
	cout << " cell is double safe." << endl << endl;
	cout << "Press enter to return to the game" << endl;
	ClearScreen();
}
// CreateBoard
// Creates a 2-dimensional array of play_cells. Around the array a border is placed
// which helps us in avoiding checking for array bounds.
playboard CreateBoard(int board_width, int board_height) {

playboard board=new play_cell*[board_height+2];
	// create rows and init per row
	int i;
	for (i=0; i<board_height+2; i++) {
		board[i]=new play_cell[board_width+2];
		// mark left and right edges as invalid
		board[i][0].status=invalid;
		board[i][board_width+1].status=invalid;
		// decrease number of closed neighbours for cells with left or right border
		board[i][1].neighbours_closed=5;
		board[i][board_width].neighbours_closed=5;
	}
	// special case : the 4 corners
	board[1][1].neighbours_closed=3;
	board[board_height][1].neighbours_closed=3;
	board[1][board_width].neighbours_closed=3;
	board[board_height][board_width].neighbours_closed=3;
	
	for (i=1; i<board_height+1; i++)
		for (int j=1; j<board_width+1; j++) {
			board[i][j].row=i;
			board[i][j].colomn=j;
		}
	return board;
}
void PlaceMines(playboard board,int board_width,int board_height,int mines_requested) {
	int rand_num;
	int colomn;
	int row;
	int mines_placed=0;
	int repetition=0;
	int total_cells=board_width*board_height;
	srand( (unsigned)time( NULL ) );
	
	while (mines_placed < mines_requested) { // hope we don't loop forever
		rand_num=rand()%total_cells;
		row = (rand_num/board_width)+1; // +1 because of the border
		colomn = (rand_num%board_width)+1;
	
		if ( board[row][colomn].mine == false) { // if we haven't placed a mine here before place mine
			board[row][colomn].mine=true;
			// Update neighbours
			for (int i=0; i<=7; i++)
				NEIGHBOUR(i)->neighbour_mines++;
			
			mines_placed++;
		}
		else { // incase our sequence is shorter than the number of mines needed
			repetition++;
			if (repetition > 1000)
			srand( (unsigned)time( NULL ) ); // place new seed
		}
	}
}


// PrintBoards
// This prints the play board and the analysis board next to eachother
// on the console.
void PrintBoards(playboard board,int board_width,int board_height,Hintfield hintField,int mark_column=0, Mode mode = opening) {
	int q,power,width;
	width = (int)log10(board_height) + 1;

	// print caption
	cout << setw(width+1) << " " << setw(board_width) << setiosflags(ios::left)<< "Playfield" << setw(width+5) << " " << "Hints - ";

	switch (hintField) {
		case rulesField:
		cout << "rule based";
		break;
		case heuristicField:
		cout << "heuristic based" << endl;
		break;
	}

	cout << resetiosflags(ios::left) << endl;

	// Print column numbers, one 1 line for each digit
	for (q=(int)log10(board_width); q>=0; q--) {
		cout << setw(width) << " "; // offset used by the row numbers
		power = (int)pow(10,q);
		int r;
		for (r=0; r<board_width+1; r++) {
			if ( r/power > 0)
				cout << (r/power)%10;
			else
				cout << " ";
		}
		cout << " "; // spacing between boards
		cout << setw(width) << " "; // offset used by the row numbers
		for (r=0; r<board_width+1; r++) {
			if ( r/power > 0)
				cout << (r/power)%10;
			else
				cout << " ";
		}
		cout << endl;
	}

// Print normal view on playboard
	for (int i=0; i<=board_height+1; i++) {
		// print row numbers
		if (i != 0 && i != board_height+1)
			cout << setw(width) << i;
		else
			cout << setw(width) << " ";

		int j;
		for (j=0; j<=board_width+1; j++) {
			switch (board[i][j].status) {
				case closed:
					if (j == mark_column)
						cout << '<';
					else
						cout << '#';
				break;
				
				case opened:
					if ( board[i][j].neighbour_mines == 0)
						cout << '.';
					else
						cout << board[i][j].neighbour_mines;
				break;

				case marked:
					if (j == mark_column && mode == marking)
						cout << 'M';
					else
						cout << 'm';
				break;

				case invalid: // visualize border,used for testing too.
					cout << '*';
				break;
			} // switch status
		} // for all colomns in one row

	cout << " "; // spacing between boards

// print row numbers
	if (i != 0 && i != board_height+1)
		cout << setw(width) << i;
	else
		cout << setw(width) << " ";


//Print special view on playboard augmented with hints
	for (j=0; j<=board_width+1; j++) {

		switch (hintField) {
	
			case rulesField:
				switch (board[i][j].status) {
					case closed:
						switch (board[i][j].hint) {
							case nohint:
								cout << '?';
							break;
							case ruleA:
								cout << 'A';
							break;
							case ruleB:
								cout << 'B';
							break;
							case ruleC:
								cout << 'C';
							break;
							case rulec:
								cout << 'c';
							break;
							case ruleD:
								cout << 'D';
							break;
						} // switch hint
					break;
					case opened:
						if ( board[i][j].neighbour_mines == 0)
							cout << '.';
						else
							cout << board[i][j].neighbour_mines;
					break;
					case marked:
						cout << 'm';
					break;
					case invalid: // visualize border,used for testing too.
						cout << '*';
					break;
				} // switch status
				break;

			case heuristicField:
				switch (board[i][j].status) {
					case closed:
						cout << board[i][j].rating;
					break;
					case opened:
						if ( board[i][j].neighbour_mines == 0)
							cout << '.';
						else
							cout << 'o';
					break;
					case marked:
						cout << 'M';
					break;
					case invalid: // visualize border,used for testing too.
						cout << '*';
					break;
				} // switch status
			break;
		} // switch hintField

	} // for all colomns in one row
	cout << endl;
} // for all rows

switch (hintField) {
	case rulesField:
		cout << "Hint legend : A,C,D is safe, B is mine. Press r for details" << endl;
	break;
	case heuristicField:
		cout << "Hint legend : o is opened, higher number is higher probability for mine" << endl;
break;
}

}


// PrintBoardsShowAll
// This prints the play board showing the positions of all mines and neighbours.
// Used for testing the program
void PrintBoardsShowAll(playboard board,int board_width,int board_height) {

	for (int i=0; i<=board_height+1; i++) {
		for (int j=0; j<=board_width+1; j++) {
	
			if (board[i][j].status == invalid)
				cout << '*';
			else if (board[i][j].mine==true)
				cout << '!';
			else if ( board[i][j].neighbour_mines == 0)
				cout << '.';
			else
				cout << board[i][j].neighbour_mines;
		}
	cout << endl;
	}
}


// UpdateNeighboursMarked
// This function updates all neighbours for a given cell in the situation that this cell becomes 'marked'. Each Neighbour has one less closed neighbour now, and one extra mine has been found.
void UpdateNeighboursMarked(playboard board,int row,int colomn) {
	play_cell* cell;
	for (int i=0; i<=7; i++) {
		cell=NEIGHBOUR(i);
		cell->neighbours_closed--;
		cell->neighbour_mines_found++;
	}
}


// UpdateNeighboursClosed
// This function updates all neighbours for a given cell in the situation
// that this cell becomes closed. This happens when a previous mine marking
// is removed. Each Neighbour has on more closed neighbour now, and one
// mine less has been found.
void UpdateNeighboursClosed(playboard board,int row,int colomn) {

// Update neighbour cells
	play_cell* cell;
	
	for (int i=0; i<=7; i++) {
		cell=NEIGHBOUR(i);
		cell->neighbours_closed++;
		cell->neighbour_mines_found--;
	}
}


// UpdateNeighboursOpened
// This function updates all neighbours for a given cell in the situation
// that this cell is opened.
void UpdateNeighboursOpened(playboard board,int row,int colomn) {
	play_cell* cell;
	
	// Update neighbour cells
	for (int i=0; i<=7; i++) {
		cell=NEIGHBOUR(i);
		cell->neighbours_closed--;
	}
}
// CheckSet
// Checks the area around the current cell to see whether new sub- or super
// sets have been created
void CheckSet(playboard board,int row,int colomn) {

	play_cell* cell;
	play_cell* crnt_cell=&board[row][colomn]; // current cell
	bool first=true; // true until the first closed cell is found
	bool found=false; // false until superset found
	int super_set_cntr=0; // counts possible supersets
	int sub_setsize[21]={0}; // counts encountered elements of possible subset
	int index;
	bool subset_found=false; // todo make more consistent with superset scanning
	crnt_cell->sub_set_cntr=0; // todo change to local as super_set_cntr
	int i;
	
	for (i=0; i<21; i++)
		sub_setsize[i]=0;
	
	// Update neighbour cells
	for (i=0; i<=7; i++) {
		cell=NEIGHBOUR(i);
	
		// Update info regarding sets
		if (cell->status == closed) {
			// Handle subsets
			for (int l=0; l<cell->membership_cntr; l++) {
				for (int m=0; m<crnt_cell->sub_set_cntr && !found; m++){
					// if we have already seen this possible subset
					if (cell->set_membership[l] == crnt_cell->sub_sets[m]) {
						found=true;
						index=m; // just for extra clarity
					}
				}
				if (found == false && cell->set_membership[l] != crnt_cell) {
					crnt_cell->sub_sets[crnt_cell->sub_set_cntr]=cell->set_membership[l];
					sub_setsize[crnt_cell->sub_set_cntr]=1; // first time encounter
					crnt_cell->sub_set_cntr++;
					subset_found=true;
				}
				else if (found == true) {
					sub_setsize[index]++; // so increment elements of this set encountered
					found = false; // reset
					subset_found=true;
				}
			}

			// Handle supersets
			// upon first closed neighbour, remember all (possibly zero) sets
			// this neighbour is an element of.
			if ( first == true ) {

				for (int k=0; k<cell->membership_cntr; k++) {
					if ( cell->set_membership[k] != crnt_cell ) { // don't remember ourselves
						crnt_cell->super_sets[k]=cell->set_membership[k];
						super_set_cntr++;
					}
				}
				first = false;
			}
			else { // second and following neighbours (compare with handling of subsets)

				for (int k=0; k<=7; k++) {
					for (int j=0; j<cell->membership_cntr; j++) {
						if (crnt_cell->super_sets[k] == cell->set_membership[j])
							found=true;
					}
					if (found == false) {// (possible) superset not found
						crnt_cell->super_sets[k]=NULL; // this can never be a superset anymore
						super_set_cntr--;
					}
					else
						found=false; // reset
	
				} // for
			}

			// Current neighbour cell is now part of the set defined by our cell (crnt_cell)
			// So add ourselves to its set_membership array, but only if we didn't add ourselves
			// before
			for (int n=0; n<cell->membership_cntr; n++) {
				if (cell->set_membership[n] == crnt_cell)
				found=true;
			}
			if (found == false) {
				cell->set_membership[cell->membership_cntr]=crnt_cell;
				cell->membership_cntr++;
			}
			else
				found=false;
	
		} // if neighbour cell is closed
	} // for all neighbour cells

	// All neighbours updated

	// See if we found supersets
	if (super_set_cntr > 0) { // we found some supersets
		AddSetType(crnt_cell,sub); // so we are a subset
		for (int i=0; i<=7; i++) { // Walk through all supersets
			if (crnt_cell->super_sets[i] != NULL) {
				AddSetType(crnt_cell->super_sets[i],super);// and mark them as super
				AddSet(crnt_cell->super_sets[i]->sub_sets,crnt_cell,20);
			}
		}
	}

	// See if we found subsets
	if (subset_found == true ) { // perhaps we found some subsets
		// Check to see if we encountered all elements
		for(i=0; i<=20; i++) {
			if (crnt_cell->sub_sets[i] != NULL) {
				if (crnt_cell->sub_sets[i]->neighbours_closed == sub_setsize[i]) {
					AddSetType( crnt_cell, super);
					AddSetType( crnt_cell->sub_sets[i], sub);
					AddSet(crnt_cell->sub_sets[i]->super_sets,crnt_cell,7);
				}
				else {// we didn't encounter all the elements of this set, so no subset
					crnt_cell->sub_sets[i]=NULL;
					crnt_cell->sub_set_cntr--; // one less subset (perhaps this cntr unneeded)
				}
			} // all possible subsets unequal to NULL
		} // for all possible subsets
	} // if we have found a possible subset


} // UpdateNeighboursOpened

// CheckCell

// This function checks if a rule applies to the given cell.
// It's only called to check opened cells, either when just opened
// or when something in their environment changes
// todo : mechanism for if multiple rules apply

void CheckCell( playboard board,int row, int colomn) {
	int mines;
	int found;
	int mines_left;
	int unknown;
	bool safe=true;
	bool rule=false;
	play_cell* cell;
	play_cell* s_cell_neighbour; // super of sub set neighbours
	play_cell* crnt_cell=&board[row][colomn];
	
	mines=board[row][colomn].neighbour_mines;
	found=board[row][colomn].neighbour_mines_found;
	mines_left = mines-found;
	unknown=board[row][colomn].neighbours_closed;

	// Rule A (special case)
	if ( mines == 0) { // all neighbours are surely safe
		for (int i=0; i<=7; i++) { // automaticly open them all
			cell=NEIGHBOUR(i);
			if ( cell->status == closed)
				OpenCell(board,cell->row,cell->colomn);
		}
	}
	// Rule A (General)
	else if ( mines == found ) { // all mines found

		for (int i=0; i<=7; i++) {// so all closed neighbours are safe
			cell=NEIGHBOUR(i);
			if ( cell->status == closed) {
				if ( cell->hint == rulec || cell->hint == ruleC || cell->hint == ruleD)
					cell->hint=ruleD;
				else
					cell->hint=ruleA;
			if (autoA == true)
				OpenCell(board,cell->row,cell->colomn);
			}
		}

	}
	// Rule B
	else if ( mines-found == unknown ) { // mines left equals unknown cells
		for (int i=0; i<=7; i++) {// so all closed neighbours are mines
			cell=NEIGHBOUR(i);
			if ( cell->status == closed) {
				cell->hint=ruleB;
				if (autoB == true) {
					MarkCell(board,cell->row,cell->colomn);
				}
			}
		}
	}
	// Rule C
	// Rule C comes in two versions which do exactly the same but from different
	// viewpoints. This is done to omit checking neighbours in a wider range.
	// problem " The rule is executed twice (doesn't harm though)
	// Todo : prevent second execution

	// Rule C (seen from cell defining subset)
	else {
		if ( crnt_cell->unc == sub || crnt_cell->unc == both ) {

			for (int i=0; i<=7; i++) { 
				if (crnt_cell->super_sets[i] != NULL) { // Walk through all supersets
					cell=crnt_cell->super_sets[i]; // /
					// if mines left of sub set equals mines left of super set
					// all remaining mines must be within the subset, so mark
					// all others as safe
					if (mines-found == cell->neighbour_mines - cell->neighbour_mines_found) {

					// Walk through all elements (=closed neighbours) of superset
						for (int j=0; j<=7; j++) {
							s_cell_neighbour=Neighbour(board,cell->row,cell->colomn,j);
							if (s_cell_neighbour->status == closed) {
								// Identify and mark those not being part of subset
								for (int k=0; k<s_cell_neighbour->membership_cntr; k++) {
									if (s_cell_neighbour->set_membership[k] == crnt_cell)
										safe=false;
								}
								if (safe == true) {
									if ( s_cell_neighbour->hint == ruleA || s_cell_neighbour->hint == ruleD)
										s_cell_neighbour->hint=ruleD;
									else
										s_cell_neighbour->hint=rulec;
									if (autoC == true)
										OpenCell(board,s_cell_neighbour->row,s_cell_neighbour->colomn);
								}
								else
								safe = true;
							} // if superset neighbour is closed
						} // for all elements in subset
					} // if remaining mines of sub and super set is equal
				} // if superset is not empty
			} // for all supersets
		} // Rule C

		mines=board[row][colomn].neighbour_mines;
		found=board[row][colomn].neighbour_mines_found;
		mines_left = mines-found;
		unknown=board[row][colomn].neighbours_closed;
		
		// Rule C (seen from cell defining super set)
		if ( crnt_cell->unc == super || crnt_cell->unc == both) {
	
			for (int i=0; i<=20; i++) { 
				if (crnt_cell->sub_sets[i] != NULL) { // Walk through all subsets
					cell=crnt_cell->sub_sets[i]; // /
					// if mines left of super set equals mines left of sub set
					// all remaining mines must be within the subset, so mark
					// all others as safe
					if (mines-found == cell->neighbour_mines - cell->neighbour_mines_found) {
						// Walk through all elements (=closed neighbours) of superset
						for (int j=0; j<=7; j++) {
							s_cell_neighbour=Neighbour(board,row,colomn,j);
							if (s_cell_neighbour->status == closed) {
	
								// Identify and mark those not being part of subset
								for (int k=0; k<s_cell_neighbour->membership_cntr; k++) {
									if (s_cell_neighbour->set_membership[k] == cell)
										safe=false;
								}
								if (safe == true) {
									if ( s_cell_neighbour->hint == ruleA || s_cell_neighbour->hint == ruleD)
										s_cell_neighbour->hint=ruleD;
									else
										s_cell_neighbour->hint=ruleC;
									if (autoC == true)
										OpenCell(board,s_cell_neighbour->row,s_cell_neighbour->colomn);
								}
								else
									safe = true;
							} // if superset neighbour is closed
						} // for all elements in subset
					} // if remaining mines of sub and super set is equal
				} // if superset is not empty
			} // for all supersets
		} // Rule C
	}	

	mines=board[row][colomn].neighbour_mines;
	found=board[row][colomn].neighbour_mines_found;
	mines_left = mines-found;
	unknown=board[row][colomn].neighbours_closed;
	// Heuristic
	// The heuristic is not actually a rule (IF condition THEN ...), but more an update
	// of the statistics. It could therefore just as well be placed in some updating function

	int num_tot=1,denom_tot=1;

	for (int i=0; i<=7; i++) {
		cell=NEIGHBOUR(i);
		if ( cell->status == closed) {
			cell->heur_numerator[i] = unknown - mines_left; // inverse chance
			cell->heur_denominator[i] = unknown;
			num_tot=1;
			denom_tot=1;
			for (int j=0; j<=7; j++) {
				num_tot *= cell->heur_numerator[j];
				denom_tot *= cell->heur_denominator[j];
			}
			cell->rating = int((9.0 * double(denom_tot-num_tot)/double(denom_tot))+0.5);

		}
	}
}


// CheckNeighbours
// This function is called after a cell has been changed in some way.
// We now check if the change has affected the environment such that
// a rule can be applied to the new situation.
void CheckNeighbours(playboard board,int row, int colomn) {
	play_cell* cell;

	for (int i=0; i<=7; i++) {
		cell=NEIGHBOUR(i);
		if ( cell->status == opened) {
			CheckSet(board,cell->row,cell->colomn); // needed ?
			CheckCell(board,cell->row,cell->colomn);
		}
	}

}

// OpenCell
// updates all effected cells and checks if rules apply whenever a cell
// is opened.
inline void OpenCell(playboard board,int row,int colomn) {
	if (board[row][colomn].status == closed) {
		if (board[row][colomn].mine == true)
			cout << "Computer hit a mine ! You probably marked a cell wrongly as mine" << endl;
			gcells--;
			board[row][colomn].status=opened; // update self
			UpdateNeighboursOpened(board,row,colomn); // update neighbors
			CheckSet(board,row,colomn); // update self and direct neighbors with UNC info
			CheckNeighbours(board,row,colomn); // update direct neighbors, and their neighbors with UNC info
			// + check if rules apply
			CheckCell(board,row,colomn); // check self if rules apply (unneeded,but for extra (debug) info)
	}
}

inline void MarkCell(playboard board,int row,int colomn) {
	gmines--;
	board[row][colomn].status=marked; // update self
	UpdateNeighboursMarked(board,row,colomn); // update neighbors
	CheckSet(board,row,colomn); // update self and direct neighbors with UNC info
	CheckNeighbours(board,row,colomn); // update direct neighbors, and their neighbors with UNC info
	// + check if rules apply
	// no checking if rule applies to ourself, since we are the mine right now
}

inline void CloseCell(playboard board,int row,int colomn) {
	gmines++;
	board[row][colomn].status=closed; // update self
	UpdateNeighboursClosed(board,row,colomn); // update neighbors
	CheckSet(board,row,colomn); // update self and direct neighbors with UNC info
	CheckNeighbours(board,row,colomn); // update direct neighbors, and their neighbors with UNC info
	// + check if rules apply
	CheckCell(board,row,colomn); // check self if rules apply
}

bool AskDimensions(int &row, int &colomn,play_cell** board, int max_width, int max_height,Hintfield hintfield,Mode mode = opening) {
	bool CorrectInput = false;

	while ( CorrectInput == false ) {
		cout << "Enter colom (1.." << max_width << ") or -1 to stop" << endl;
		cin>>colomn;
		if ( colomn < 1 || colomn > max_width ) {
			ClearScreen();
			if ( colomn == -1 )
				return false;
			PrintBoards(board,max_width,max_height,hintfield);
			cout << endl << "Please choose a colomn in the range of 1.." << max_width << " or -1 only !" << endl << endl;
		}
		else
			CorrectInput = true;
	}

	CorrectInput = false;
	// print special board with marker at the colomn
	ClearScreen();
	PrintBoards(board,max_width,max_height,hintfield,colomn,mode);
	cout << "Column choosen is:" << colomn;
	if (mode == marking)
		cout << " ( '<' can be marked,'M' can be demarked in this column)";
	else
		cout << " ( '<' can be opened in this column)";
	cout << endl;

	while ( CorrectInput == false ) {
		cout << "Enter row (1.." << max_height << ") or -1 to stop" << endl;
		cin>>row;
		if ( row < 1 || row > max_height ) {
			ClearScreen();
			if (row == -1 )
				return false;
			PrintBoards(board,max_width,max_height,hintfield,colomn,mode);
			cout << "Column choosen is:" << colomn << " ( valid cells marked with '<' )" << endl;
			cout << endl << "Please choose a row in the range of 1.." << max_height << " or -1 only !" << endl << endl;
		}
		else
			CorrectInput = true;
	}

	return true;
}

int main () {

	int board_width; // width of play board
	int board_height; // height of play board
	playboard board; // 2d array of play_cells
	int colomn; // colom for selecting cell
	int row; // row for selecting cell
	char choice; // action on cell
	bool stop=false; // false when game in progress
	bool correctInput = false;
	char endGame;
	Hintfield hintField = rulesField; // the kind of hints showed
	autoA=false;
	autoB=false;
	autoC=false;
	
	char buffer[100]; // istringstream would be better but not everywhere avail.
	ostrstream action(buffer,100);
	action << "none" << ends;
	info();
	cout << "Please enter the width of the board ( > 0 )" ;
	cin>>board_width;
	cout << "Please enter the height of the board ( > 0)";
	cin>>board_height;
	int cellsNr = board_width*board_height;
	correctInput = false;
	while ( correctInput == false ) {
		cout << "Please enter number of mines (1.." << cellsNr << ")";
		cin>>gmines;
		if ( gmines < 1 )
			cout << "To play a game, we need at least a single mine. Try again" << endl << endl;
		else if ( gmines > cellsNr )
			cout << "The board only has " << cellsNr << " cell" << (cellsNr > 1 ? "s" : "" ) << ". Try again" << endl << endl;
		else
			correctInput = true;
	}


	board = CreateBoard(board_width,board_height);

	PlaceMines(board,board_width,board_height,gmines);
	
	gcells = board_width*board_height - gmines;
	
	while ( stop == false ) {
	
		ClearScreen();
		cout << endl << "Mines left:" << gmines << " To be opened cells left:" << gcells << endl;
		if ( gmines == 0 && gcells == 0 )
			cout << "-=* You have won ! You can [q]uit if you want... *=-" << endl;
		PrintBoards(board,board_width,board_height,hintField);
		cout << "Auto Complete A : " << (autoA? "on" : "off") << endl;
		cout << "Auto Complete B : " << (autoB? "on" : "off");
		cout << " Last user action : " << action.str() << endl;
		cout << "Auto Complete C : " << (autoC? "on" : "off") << endl;
		cout << endl << "[o]pen,[m]ark as mine,[?] for additional commands" << endl;
		if ( gmines == 0 && gcells == 0 )
			cout << "-=* You have won ! You can [q]uit if you want... *=-" << endl;
			cin>>choice;

		switch (choice) {

			// Open a cell
			case 'o': case 'O':

			if ( AskDimensions(row,colomn,board,board_width,board_height,hintField) == true ) {

				if (board[row][colomn].status == closed) {
					if (board[row][colomn].mine == true) {
						cout << "You hit a mine !" << endl;
						PrintBoardsShowAll(board,board_width,board_height);
						stop=true;
					}
					else {
						OpenCell(board,row,colomn);
						action << "Opened cell (" << colomn << "," << row << ")" << ends;
					}
				}
				else {
					cout << "You can only try to open closed cells. Try again" << endl;
				}
			}
			break;
	
			// Mark a cell as having a mine or removes the mark
			case 'm': case 'M':

			if ( AskDimensions(row,colomn,board,board_width,board_height,hintField,marking) == true ) {

				switch (board[row][colomn].status) {
					case marked:
					CloseCell(board,row,colomn);
					action << "Unmarked cell (" << colomn << "," << row << ")" << ends;
					break;
					case closed:
						MarkCell(board,row,colomn);
						action << "Marked cell (" << colomn << "," << row << ")" << ends;
						break;
					case opened:
						cout << "This cell is already opened..." << endl;
						break;
				}
			}
			break;

			// Print a board with all mines
			case 's': case 'S': // for cheating or testing
				PrintBoardsShowAll(board,board_width,board_height);
				
			break;

			// Let computer open all cells marked as safe according to rule A
			case 'a': case 'A': // for cheating or testing
				autoA=!autoA;
			break;

			// Let computer mark all cells as mine that ar safe according to rule B
			case 'b': case 'B': // for cheating or testing
				autoB=!autoB;
			break;

			// Let computer open all cells marked as safe according to rule C
			case 'c': case 'C': // for cheating or testing
				autoC=!autoC;
			break;

			case 'q' : case 'Q':
				cout << "This will quit to game. Press [y] if you really want this." << endl;
				cin>>endGame;
				if ( endGame == 'Y' || endGame == 'y' )
					stop = true;
			break;
			// Display help menu
			case '?':
				help();
			break;

			case 'r': case 'R' :
				rules();
			break;

			case 'h': case 'H' :
				if (hintField == rulesField)
					hintField = heuristicField;
				else
					hintField = rulesField;
			break;

			default:
				cout << choice << " is not a valid option. Try again or press ? for help." << endl;
			break;


		}
	}
	cout << "Bye!" << endl;
return 0;
} 
