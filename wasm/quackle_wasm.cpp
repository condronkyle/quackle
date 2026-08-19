/*
 * Quackle WASM Bindings
 * Exposes the Quackle engine to JavaScript via Emscripten embind.
 * No Qt dependencies — pure C++ with std:: file I/O.
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cctype>

#include "alphabetparameters.h"
#include "boardparameters.h"
#include "computerplayer.h"
#include "computerplayercollection.h"
#include "datamanager.h"
#include "enumerator.h"
#include "evaluator.h"
#include "game.h"
#include "gameparameters.h"
#include "generator.h"
#include "lexiconparameters.h"
#include "move.h"
#include "rack.h"
#include "sim.h"
#include "strategyparameters.h"

#include "../test/crossplayboards.h"

// Avoid using namespace std — conflicts with emscripten::function vs std::function

// ---------- Pure-C++ alphabet loader (replaces Qt FlexibleAlphabetParameters) ----------

class WasmAlphabetParameters : public Quackle::AlphabetParameters
{
public:
    bool load(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "Cannot open alphabet file: " << filename << std::endl;
            return false;
        }

        Quackle::Letter letter = QUACKLE_FIRST_LETTER;
        std::string line;
        while (std::getline(file, line))
        {
            while (!line.empty() && isspace((unsigned char)line.back()))
                line.pop_back();
            if (line.empty() || line[0] == '#')
                continue;

            std::istringstream iss(line);
            std::string text;
            iss >> text;

            bool isBlank = (text.size() >= 5 &&
                            (text.substr(0, 5) == "blank" || text.substr(0, 5) == "Blank"));

            if (isBlank)
            {
                int score, count;
                if (!(iss >> score >> count))
                {
                    std::cerr << "Bad blank spec" << std::endl;
                    continue;
                }
                setCount(QUACKLE_BLANK_MARK, count);
                setScore(QUACKLE_BLANK_MARK, score);
            }
            else
            {
                std::string blankText;
                int score, count, vowel;
                if (!(iss >> blankText >> score >> count >> vowel))
                {
                    std::cerr << "Bad letter spec for: " << text << std::endl;
                    continue;
                }
                UVString textUV = MARK_UV(text);
                UVString blankTextUV = MARK_UV(blankText);
                setLetterParameter(letter,
                    Quackle::LetterParameter(letter, textUV, blankTextUV, score, count, vowel != 0));
                ++letter;
            }
        }
        return true;
    }
};

// ---------- Engine wrapper ----------

static Quackle::DataManager *s_dm = nullptr;

std::string moveActionStr(Quackle::Move::Action a)
{
    switch (a)
    {
    case Quackle::Move::Place:       return "place";
    case Quackle::Move::Exchange:    return "exchange";
    case Quackle::Move::BlindExchange: return "exchange";
    case Quackle::Move::Pass:        return "pass";
    default:                         return "other";
    }
}

std::string positionString(const Quackle::Move &m)
{
    if (m.action != Quackle::Move::Place)
        return "";
    int row = m.startrow;
    int col = m.startcol;
    char colChar = 'A' + col;
    if (m.horizontal)
        return std::to_string(row + 1) + std::string(1, colChar);
    else
        return std::string(1, colChar) + std::to_string(row + 1);
}

std::string tilesToString(const Quackle::LetterString &ls)
{
    std::string result;
    for (auto ch : ls)
    {
        if (ch == QUACKLE_PLAYED_THRU_MARK)
        {
            result += '.';
        }
        else if (ch >= QUACKLE_BLANK_OFFSET)
        {
            result += (char)tolower('A' + (ch - QUACKLE_BLANK_OFFSET - QUACKLE_FIRST_LETTER));
        }
        else if (ch >= QUACKLE_FIRST_LETTER)
        {
            result += (char)('A' + (ch - QUACKLE_FIRST_LETTER));
        }
        else if (ch == QUACKLE_BLANK_MARK)
        {
            result += '?';
        }
    }
    return result;
}

Quackle::Letter charToLetter(char c)
{
    if (c >= 'A' && c <= 'Z')
        return QUACKLE_FIRST_LETTER + (c - 'A');
    if (c >= 'a' && c <= 'z')
        return QUACKLE_BLANK_OFFSET + QUACKLE_FIRST_LETTER + (c - 'a');
    return QUACKLE_NULL_MARK;
}

// ---------- JS-facing API ----------

bool initEngine(std::string dataDir)
{
    if (s_dm)
        delete s_dm;

    s_dm = new Quackle::DataManager();

    s_dm->setAppDataDirectory(dataDir);
    s_dm->setComputerPlayers(Quackle::ComputerPlayerCollection::fullCollection());

    std::string alphabetFile = Quackle::AlphabetParameters::findAlphabetFile("crossplay");
    if (alphabetFile.empty())
    {
        std::cerr << "Cannot find crossplay alphabet file" << std::endl;
        return false;
    }

    auto *alpha = new WasmAlphabetParameters();
    if (!alpha->load(alphabetFile))
    {
        delete alpha;
        return false;
    }
    alpha->setAlphabetName("crossplay");
    s_dm->setAlphabetParameters(alpha);

    s_dm->setBoardParameters(new CrossplayBoard());
    s_dm->setParameters(new Quackle::CrossplayParameters());

    std::string dawgFile = Quackle::LexiconParameters::findDictionaryFile("nwl23.dawg");
    std::string gaddagFile = Quackle::LexiconParameters::findDictionaryFile("nwl23.gaddag");

    if (dawgFile.empty())
    {
        std::cerr << "Cannot find nwl23.dawg" << std::endl;
        return false;
    }
    s_dm->lexiconParameters()->loadDawg(dawgFile);

    if (!gaddagFile.empty())
        s_dm->lexiconParameters()->loadGaddag(gaddagFile);
    else
        std::cerr << "Warning: GADDAG not found, move generation will be slower" << std::endl;

    s_dm->setBackupLexicon("default_english");
    s_dm->strategyParameters()->initialize("nwl23");

    std::cout << "Quackle WASM engine initialized" << std::endl;
    return true;
}

bool prepareGame(std::string gridJson, std::string rackStr, int playerScore, int oppScore, int numMoves,
				 int requestedBagCount, int requestedFinalTurns, bool requireObservedState,
				 Quackle::Game &game, std::string *error)
{
    if (numMoves <= 0)
    {
        *error = "Move count must be positive";
        return false;
    }

    std::vector<std::string> gridRows;
    {
        std::string s = gridJson;
        size_t pos = s.find('[');
        if (pos == std::string::npos)
        {
            *error = "Invalid grid JSON";
            return false;
        }
        s = s.substr(pos + 1);
        while (true)
        {
            size_t q1 = s.find('"');
            if (q1 == std::string::npos) break;
            size_t q2 = s.find('"', q1 + 1);
            if (q2 == std::string::npos) break;
            gridRows.push_back(s.substr(q1 + 1, q2 - q1 - 1));
            s = s.substr(q2 + 1);
        }
    }

    if (gridRows.size() != 15)
    {
        *error = "Grid must have exactly 15 rows, got " + std::to_string(gridRows.size());
        return false;
    }
    for (const auto &row : gridRows)
    {
        if (row.size() != 15)
        {
            *error = "Each grid row must have exactly 15 cells";
            return false;
        }
    }
    if (rackStr.empty() || rackStr.size() > (size_t)QUACKLE_PARAMETERS->rackSize())
    {
        *error = "Rack must contain 1 to " + std::to_string(QUACKLE_PARAMETERS->rackSize()) + " tiles";
        return false;
    }

    Quackle::PlayerList players;
    Quackle::Player p1(MARK_UV("You"), Quackle::Player::HumanPlayerType, 0);
    Quackle::Player p2(MARK_UV("Opp"), Quackle::Player::HumanPlayerType, 1);
    p1.setScore(playerScore);
    p2.setScore(oppScore);
    players.push_back(p1);
    players.push_back(p2);
    game.setPlayers(players);

    game.addPosition();
    Quackle::GamePosition &pos = game.currentPosition();

    // Place tiles on the board
    Quackle::Board &board = pos.underlyingBoardReference();
    board.prepareEmptyBoard();
    Quackle::Bag boardRemainder;

    bool hasTiles = false;
    for (int row = 0; row < 15; row++)
    {
        for (int col = 0; col < 15 && col < (int)gridRows[row].size(); col++)
        {
            char c = gridRows[row][col];
            if (c != '.')
            {
                Quackle::Letter letter = charToLetter(c);
                if (letter == QUACKLE_NULL_MARK)
                {
                    *error = "Grid contains an invalid tile symbol";
                    return false;
                }
                const bool isBlank = c >= 'a' && c <= 'z';
                if (!boardRemainder.removeLetter(isBlank ? QUACKLE_BLANK_MARK : letter))
                {
                    *error = "Board contains more tiles than the Crossplay distribution";
                    return false;
                }
                board.setLetterAt(row, col, letter);
                board.setIsBlankAt(row, col, isBlank);
                hasTiles = true;
            }
        }
    }

    if (hasTiles)
        board.setNotEmpty();

    // Compute cross-checks using Generator
    {
        Quackle::Generator gen(pos);
        gen.allCrosses();
        pos.setBoard(gen.position().board());
    }

    // Game::addPosition deals random racks. Clear them before rebuilding the
    // position from the observed board and known rack.
    pos.setPlayerRack(0, Quackle::Rack(), false);
    pos.setPlayerRack(1, Quackle::Rack(), false);

    // The board was loaded directly, so rebuild the bag before assigning the
    // known rack. Unknown opponent tiles remain in the bag until each playout.
    pos.setBag(boardRemainder);

    Quackle::LetterString rackLetters;
    for (char c : rackStr)
    {
        Quackle::Letter l = charToLetter((char)toupper((unsigned char)c));
        if (c == '?' || c == '_')
            rackLetters.push_back(QUACKLE_BLANK_MARK);
        else if (l != QUACKLE_NULL_MARK)
            rackLetters.push_back(l);
        else
        {
            *error = "Rack contains an invalid tile symbol";
            return false;
        }
    }
    const Quackle::Rack currentRack(rackLetters);
    if (!pos.canSetCurrentPlayerRackWithoutBagExpansion(currentRack))
    {
        *error = "Rack or board contains more tiles than the Crossplay distribution";
        return false;
    }
	pos.setCurrentPlayerRack(currentRack);

	if (requireObservedState)
	{
		const int unseenCount = pos.bag().size();
		if (requestedBagCount < 0 || requestedBagCount > unseenCount)
		{
			*error = "Tile bag count is inconsistent with the board and rack";
			return false;
		}

		const int hiddenRackCount = unseenCount - requestedBagCount;
		if (hiddenRackCount < 0 || hiddenRackCount > QUACKLE_PARAMETERS->rackSize())
		{
			*error = "Opponent rack size is inconsistent with the tile bag count";
			return false;
		}
		if (requestedBagCount > 0 && hiddenRackCount != QUACKLE_PARAMETERS->rackSize())
		{
			*error = "A nonempty tile bag requires seven hidden opponent tiles";
			return false;
		}
		if (requestedBagCount > 0 && requestedFinalTurns != 0)
		{
			*error = "Final-turn state is invalid while tiles remain in the bag";
			return false;
		}
		if (requestedBagCount == 0 && requestedFinalTurns != 1 && requestedFinalTurns != 2)
		{
			*error = "Choose whether one or two Crossplay final turns remain";
			return false;
		}
	}

    // Candidate generation must see the real bag size. Unknown opponent tiles
    // are hidden, but they are on the opponent rack rather than in the bag.
	Quackle::Simulator::randomizeOppoRacks(pos, Quackle::Rack());
	if (requireObservedState)
	{
		if (pos.bag().size() != requestedBagCount)
		{
			*error = "Tile bag count does not match the reconstructed position";
			return false;
		}
		if (!pos.setFinalTurnsRemaining(requestedFinalTurns))
		{
			*error = "Final-turn state does not match the reconstructed position";
			return false;
		}
	}

    // Generate moves
    pos.kibitz(numMoves);

    if (pos.moves().empty())
    {
        *error = "Quackle generated no candidate moves";
        return false;
    }

    return true;
}

emscripten::val serializeMoves(const Quackle::MoveList &moveList, const Quackle::Rack &currentRack,
                               const Quackle::Simulator *simulator)
{
    emscripten::val movesArray = emscripten::val::array();
    int i = 0;
    for (const auto &m : moveList)
    {
        emscripten::val moveObj = emscripten::val::object();
        moveObj.set("tiles", tilesToString(m.prettyTiles()));
        moveObj.set("position", positionString(m));
        moveObj.set("score", m.effectiveScore());
        moveObj.set("equity", m.equity);
		if (simulator)
		{
			const Quackle::SimmedMove &simmed = simulator->simmedMoveForMove(m);
			moveObj.set("win", m.win);
			moveObj.set("samples", (int)simmed.wins.incorporatedValues());
			moveObj.set("terminalSamples", (int)(simmed.terminalResults.averagedValue()
				* simmed.terminalResults.incorporatedValues() + 0.5));
		}
		else
		{
			moveObj.set("win", emscripten::val::null());
			moveObj.set("samples", 0);
			moveObj.set("terminalSamples", 0);
        }
        moveObj.set("leave", tilesToString((currentRack - m).tiles()));
        moveObj.set("action", moveActionStr(m.action));
        movesArray.set(i++, moveObj);
    }

    return movesArray;
}

emscripten::val kibitz(std::string gridJson, std::string rackStr, int playerScore, int oppScore, int numMoves)
{
    emscripten::val result = emscripten::val::object();
    result.set("moves", emscripten::val::array());

    if (!s_dm || !s_dm->isGood())
    {
        result.set("error", std::string("Engine not initialized"));
        return result;
    }

    Quackle::Game game;
    std::string error;
	if (!prepareGame(gridJson, rackStr, playerScore, oppScore, numMoves,
		-1, 0, false, game, &error))
    {
        result.set("error", error);
        return result;
    }

    Quackle::GamePosition &pos = game.currentPosition();
    result.set("moves", serializeMoves(pos.moves(), pos.currentPlayer().rack(), nullptr));
    result.set("simulated", false);
    result.set("error", std::string(""));
    return result;
}

emscripten::val simulateKibitz(std::string gridJson, std::string rackStr, int playerScore, int oppScore,
							  int numMoves, int iterations, int bagCount, int finalTurnsRemaining)
{
	// Candidate plus two future moves can draw at most three full racks.
	const int terminalSimulationBagLimit = QUACKLE_PARAMETERS->rackSize() * 3;
	const bool playToEnd = bagCount >= 0 && bagCount <= terminalSimulationBagLimit;
	const int plies = playToEnd ? -1 : 2;
    emscripten::val result = emscripten::val::object();
    result.set("moves", emscripten::val::array());
    result.set("simulated", false);
    result.set("plies", plies);
	result.set("iterationsRequested", iterations);
	result.set("iterationsCompleted", 0);
	result.set("bagCount", bagCount);
	result.set("finalTurnsRemaining", finalTurnsRemaining);
	result.set("simulationMode", std::string(playToEnd ? "play-to-end" : "2-ply"));
	result.set("playToEnd", playToEnd);

    if (!s_dm || !s_dm->isGood())
    {
        result.set("error", std::string("Engine not initialized"));
        return result;
    }
    if (iterations <= 0 || iterations > 230)
    {
        result.set("error", std::string("Simulation iterations must be between 1 and 230"));
        return result;
    }
    Quackle::Game game;
    std::string error;
	if (!prepareGame(gridJson, rackStr, playerScore, oppScore, numMoves,
		bagCount, finalTurnsRemaining, true, game, &error))
    {
        result.set("error", error);
        return result;
    }

    Quackle::GamePosition &pos = game.currentPosition();
    const Quackle::Rack currentRack = pos.currentPlayer().rack();

    try
    {
        Quackle::Simulator simulator;
        simulator.setThreadCount(0);
        simulator.setPosition(pos);
        simulator.setIgnoreOppos(false);
        simulator.simulate(plies, iterations);

        const Quackle::MoveList moveList = simulator.moves();
		const int completed = simulator.iterations();
		const int candidateCount = (int)moveList.size();
		bool allTerminal = true;
		for (const auto &move : moveList)
		{
			const Quackle::SimmedMove &simmed = simulator.simmedMoveForMove(move);
			const int terminalSamples = (int)(simmed.terminalResults.averagedValue()
				* simmed.terminalResults.incorporatedValues() + 0.5);
			if (terminalSamples != completed)
				allTerminal = false;
		}

		result.set("moves", serializeMoves(moveList, currentRack, &simulator));
		result.set("simulated", completed == iterations && (!playToEnd || allTerminal));
        result.set("iterationsCompleted", completed);
        result.set("candidateCount", candidateCount);
        result.set("totalPlayaheads", completed * candidateCount);
		result.set("winModel", std::string(playToEnd ? "terminal" : "bogowin"));
		result.set("error", playToEnd && !allTerminal
			? std::string("A sampled endgame did not reach the official Crossplay finish")
			: std::string(""));
    }
    catch (const std::exception &e)
    {
        result.set("error", std::string("Simulation failed: ") + e.what());
    }

    return result;
}

bool isWordValid(std::string word)
{
    if (!s_dm || !s_dm->isGood())
        return false;

    Quackle::LetterString ls;
    for (char c : word)
    {
        Quackle::Letter l = charToLetter(toupper(c));
        if (l != QUACKLE_NULL_MARK)
            ls.push_back(l);
    }

    // Use Generator's isAcceptableWord which uses the DAWG properly
    Quackle::Generator gen;
    return gen.isAcceptableWord(ls);
}

std::string getEngineInfo()
{
    if (!s_dm)
        return "Not initialized";

    std::string info = "Quackle WASM Engine\n";
    info += "Alphabet: " + QUACKLE_ALPHABET_PARAMETERS->alphabetName() + "\n";
    info += "DAWG loaded: " + std::string(QUACKLE_LEXICON_PARAMETERS->hasDawg() ? "yes" : "no") + "\n";
    info += "GADDAG loaded: " + std::string(QUACKLE_LEXICON_PARAMETERS->hasGaddag() ? "yes" : "no") + "\n";
    info += "Board: " + std::to_string(QUACKLE_BOARD_PARAMETERS->width()) + "x"
                      + std::to_string(QUACKLE_BOARD_PARAMETERS->height()) + "\n";
    info += "Bingo bonus: " + std::to_string(QUACKLE_PARAMETERS->bingoBonus()) + "\n";
    return info;
}

EMSCRIPTEN_BINDINGS(quackle)
{
    emscripten::function("initEngine", &initEngine);
    emscripten::function("kibitz", &kibitz);
    emscripten::function("simulateKibitz", &simulateKibitz);
    emscripten::function("isWordValid", &isWordValid);
    emscripten::function("getEngineInfo", &getEngineInfo);
}
