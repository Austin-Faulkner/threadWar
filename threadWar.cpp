/*
	TEAM:
	 - Andy Herr, jah534
	 - Austin Faulkner, a_f408
*/

#include <ostream>
#include <iostream>
#include <deque>		//Stores the card deck; allows easy draw and discard, as well as quick draw and discard.
#include <algorithm>	//shuffle()
#include <ctime>		//Seeding random
#include <random>		//Random number generator
#include <pthread.h>
#include <iterator>
#include <fstream>
#include <array>

typedef std::size_t s_t;

const s_t NUM_PLAYERS = 3;
const s_t DEALER = 1;
const s_t ROUND_LIMIT = 5;
const s_t GAME_LIMIT = 3;
enum ThreadState {WAIT, GO, DEAL, PLAY, NEXT_TURN, WIN, EXIT_ROUND, TERMINATE, ERROR};

std::deque<s_t> deck = 	{101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
			   				 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
							    301, 302, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 313,
							    401, 402, 403, 404, 405, 406, 407, 408, 409, 410, 411, 412, 413};

//Stuff to make deck shuffling and random discard easier
s_t seed = std::time(0);

//Random number generator for std::shuffle()
auto rng = std::default_random_engine {static_cast<unsigned>(seed)};

//Simply call this function when you want to shuffle the deck.
//No need to pass arguments
void shuffleDeck()
{
	std::shuffle(deck.begin(), deck.end(), rng);
}

//Call this when selecting which card from the hand you want to discard.
//Returns either 0 or 1, since the hand will have at most 2 cards.
s_t randomCard()
{
	return rand() % 2;
}

std::array<ThreadState, DEALER + NUM_PLAYERS> threads;

using std::ofstream;
ofstream outdata;

s_t turnPlayer, startPlayer;

typedef struct Argument
{
	s_t ownID;
} Argument;

//std::array<Argument, NUM_PLAYERS+1> argStructs;
Argument argStructs[NUM_PLAYERS + 1];

void* playerFunction(void* args);



int main(int argc, char *argv[])
{
	int rc;
	s_t turnPlayer, startPlayer;
	Argument* currentArg;	//For passing into each player thread.

	//Open the output file BEFORE making any thread
	outdata.open("log.txt");

	//Initialize all player scores.
	//scores[0] is unused.
	std::array<s_t, NUM_PLAYERS + 1> scores;
	scores.fill(0);

	//We're in main thread; only need to allocate players.
	//Storing them in an array will make the loop below work.
	//To make id'ing them easier, threadIDs[0] will just be empty and unused.
	std::array<pthread_t, NUM_PLAYERS + 1> threadIDs;

	//Set all player thread states to WAIT
	threads.fill(WAIT);

	//After that, change main thread state to go.
	threads[0] = GO;

	//Create player threads, passing threadId and each player's starting card to them.
	for (s_t targetID = 1; targetID <= NUM_PLAYERS; targetID++)
	{
		currentArg = &argStructs[targetID];

		//Give target thread's id to its function as an argument.
		currentArg->ownID = targetID;

		rc = pthread_create(&threadIDs[targetID], NULL, playerFunction, (void *) currentArg);
		if (rc)
		{
         //Close output file and exit
		  	outdata.close();
         exit(-1);
    	}
	}

	//Pre-game initialization
	turnPlayer = 1;
	startPlayer = 1;
	s_t gameCounter = 0;

	while (gameCounter < GAME_LIMIT)
	{
		s_t roundCounter = 0;
		turnPlayer = startPlayer;

		//New game message.
		std::cout << "\nNew Game " << gameCounter + 1 << ":\n";
		bool continueGame = true;


		//Shuffle deck and deal one card to each player.
		outdata << "DEALER: shuffle\n";
		shuffleDeck();

		for (s_t targetThread = 1; targetThread <= NUM_PLAYERS; targetThread++)
		{
			threads[0] = WAIT;
			threads[targetThread] = DEAL;
			while (!threads[0]);

			if (threads[0] == ERROR)
			{
				outdata.close();
				exit(-1);
			}
		}

		outdata << "\nBegin drawing.\n";
		//Game loop:
		while (continueGame)
		{
			if (turnPlayer == startPlayer)
				roundCounter++;

			if (roundCounter > ROUND_LIMIT)
			{
				//Send Draw message to log file
				outdata << "The round is a draw!\n";

				for (s_t targetID = 1; targetID <= NUM_PLAYERS; targetID++)
				{
					//Change dealer's state to WAIT
					threads[0] = WAIT;
					//Change player's state to EXIT_ROUND
					threads[targetID] = EXIT_ROUND;
					//Do nothing while dealer's state is WAIT.
					while (!threads[0]);

					//Error checking, just in case
					if (threads[0] == ERROR)
					{
						outdata.close();
						exit(-1);
					}
				}
				outdata << std::endl;

				//Increment gameCounter
				gameCounter++;

				//Break out of game loop
				break;
			}

			//Set dealer's state to WAIT.
			threads[0] = WAIT;
			//Set turn player's state to PLAY.
			threads[turnPlayer] = PLAY;

			//Wait for turn player's thread to finish and change dealer state.
			while (!threads[0]);

			switch (threads[0])
			{
				//if state is NEXT_TURN:
				case NEXT_TURN:

					//Send contents of deck to log file
					outdata << "DECK: ";
					for (auto it = deck.begin(); it != deck.end(); it++)
						outdata << *it << " ";
					outdata << "\n\n";

					//turnPlayer becomes next in cycle
					turnPlayer = (turnPlayer % NUM_PLAYERS) + 1;
					break;

				//if state is WIN:
				case WIN:

					//Add to turnPlayer's score counter.
					scores[turnPlayer]++;

					//For each player:
					for (s_t player = 1; player <= NUM_PLAYERS; player++)
					{
						//Change dealer's state to WAIT
						threads[0] = WAIT;

						//Change player's state to EXIT_ROUND
						threads[player] = EXIT_ROUND;

						//Do nothing while dealer's state is WAIT.
						//Loop will continue once the player changes dealer's state to GO.
						while (!threads[0]);

						//Error checking
						if (threads[0] == ERROR)
						{
							outdata.close();
							exit(-1);
						}
					}

					outdata << std::endl;

					//Increment gameCounter
					gameCounter++;

					continueGame = false;
					break;

				//if state is anything else:
				default:
				 	//Print error message.
				 	std::cerr << "Error: Invalid state\n";

					//Perform cleanup and exit.
					outdata.close();
					exit(-1);
				 	break;
			}
		}

		//Show deck of cards at the end of the round
		std::cout << "DECK: ";
		for (auto it = deck.begin(); it != deck.end(); it++)
			std::cout << *it << " ";
		std::cout << "\n\n";

		//Display current scores.
		std::cout << "\nCurrent scores:\n";
		for (s_t score_i = 1; score_i <= NUM_PLAYERS; score_i++)
		{
			std::cout << "PLAYER " << score_i << ": " << scores[score_i] << std::endl;
		}

		//Start player becomes next in cycle.
		startPlayer = (startPlayer % NUM_PLAYERS) + 1;
	}

	//Game over. Terminate player threads, peform cleanup and exit
	for (s_t thread = 1; thread <= NUM_PLAYERS; thread++)
	{
		threads[0] = WAIT;
		threads[thread] = TERMINATE;
		while(!threads[0]);
	}
	outdata.close();
	pthread_exit(NULL);
}

////////////////////////////////////////////////////////////////////
/////////the playerFunction() which serves to play the game/////////
////////////////////////////////////////////////////////////////////

void* playerFunction(void* args)
{
	Argument* argStruct = (Argument*) args;
	s_t id = argStruct->ownID;

	//Hand is reperesented by Dequeue container.
	std::deque<s_t> playerHand;

	//Inline helper functions

	//Generate string representing player hand
	auto showHand = [&playerHand]()
	{
		std::string handString = "";
		for (s_t i = 0; i < playerHand.size(); i++)
		{
			handString += (std::to_string(playerHand[i]) + " ");
		}
		//Remove the last space
		if (handString.size() > 0)
		{
			handString.pop_back();
		}
		return handString;
	};

	//Draw a card and log it.
	auto drawCard = [&playerHand, &id]() mutable
	{
		s_t newCard = deck.front();
		playerHand.push_back(newCard);
		outdata << "Player " << id << ": draws " << newCard << std::endl;
		deck.pop_front();
	};

	//Discard a random card to bottom of deck and log it.
	auto discardRandom = [&playerHand, &id]() mutable
	{
		s_t cardIndex = randomCard();
		s_t oldCard = playerHand[cardIndex];
		outdata << "PLAYER " << id << ": discards " << oldCard << std::endl;
		deck.push_back(oldCard);
		if (cardIndex /*==1, aka the back*/)
			playerHand.pop_back();
		else
			playerHand.pop_front();
	};

	bool keepPlaying = true;

	//while keepPlaying:
	while (keepPlaying)
	{
		//Wait for thread state to change from WAIT
		while (!threads[id]);

		switch (threads[id])
		{
			case DEAL:
				//Simply draw a card. Assignment doesn't say to log it, but it won't hurt.
				drawCard();
				threads[id] = WAIT;
				threads[0] = GO;
				break;

			case PLAY:
				drawCard();

				//Send hand to log file
				outdata << "player " << id << ": hand " << showHand() << std::endl;

				//If cards have same value:
				if (playerHand[0]%100 == playerHand[1]%100)
				{
					//Send win message to log file
					//Screen message will be handled in EXIT_ROUND case.
					outdata << "PLAYER " << id << ": wins" << std::endl;

					//Change own state to WAIT
					threads[id] = WAIT;

					//Change main thread state to WIN
					threads[0] = WIN;
			 	}
				else
			   {
					//Discard random card and log it.
					discardRandom();

					//Send hand to log file
					outdata << "PLAYER " << id << ": hand " << showHand() << std::endl;

					//Change own state to WAIT
					threads[id] = WAIT;

					//Change main thread state to NEXT_TURN
					threads[0] = NEXT_TURN;
			   }
			   break;

		case EXIT_ROUND:
			//Send exit round message to log file
			outdata << ("PLAYER " + std::to_string(id) + ": exits round\n");

			//Send status to screen
			std::cout << ("PLAYER " + std::to_string(id) + ":\n");
			std::cout << ("HAND: " + showHand() + '\n');
			std::cout << "WIN: ";
			if (playerHand.size() == 2)
				std::cout << "yes\n";
			else
				std::cout << "no\n";

			//Put cards back in the deck
			while (playerHand.size() > 0)
			{
				deck.push_back(playerHand[0]);
			 	playerHand.pop_front();
			}

			//Change own state to WAIT
			threads[id] = WAIT;

			//Change main thread state to GO
			threads[0] = GO;
			break;

		case TERMINATE:
			while (playerHand.size() > 0)
			{
				deck.push_back(playerHand[0]);
				playerHand.pop_front();
			}
			//Send termination message to log file
			outdata << "PLAYER " << id << ": terminates\n";

			//Change main thread state to GO
			threads[0] = GO;

			//Exit game loop
			keepPlaying = false;
			break;

		default:
			//Log error message.
			outdata << "An unexpected error occurred.\n";
			//Something's weird, tell main to shut it all down.
			threads[0] = ERROR;
			keepPlaying = false;
			break;
		}
	}
	//Exit thread
	pthread_exit(NULL);
}

// RESOURCES:

// https://www.ibm.com/docs/en/zos/2.4.0?topic=functions-pthread-create-create-thread
