//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "NAME";
const char *studentID   = "PID";
const char *email       = "EMAIL";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here
//
uint32_t globalHistory;   // Store the most recent branch outcomes.
uint32_t historyMask;     // Keep only the low ghistoryBits bits.
uint8_t *counterTable;    // Store one 2-bit saturating counter per entry.

static uint32_t localHistoryMask;
static uint32_t pcIndexMask;
static uint32_t *localHistoryTable;
static uint8_t *localCounterTable;
// Choice states 0/1 select global; states 2/3 select local.
static uint8_t *choiceTable;

// Custom: PC-indexed perceptrons with 30 global-history inputs and a bias.
// Storage used by Custom (other predictor tables are not allocated):
//   customWeights: 256 * 31 * 8 = 63,488 bits (signed, saturating weights).
//   customHistory: 32 bits, of which the low 30 contain branch history.
//   customScore: 32 bits, retained between prediction and training.
// Total: 63,552 bits, below the 65,792-bit limit.
// Dimensions, masks and threshold are constants, not stored adaptive state.
#define CUSTOM_ENTRIES 256
#define CUSTOM_HISTORY_BITS 30
#define CUSTOM_THRESHOLD 72
#define CUSTOM_HISTORY_MASK UINT32_C(0x3fffffff)
static int8_t customWeights[CUSTOM_ENTRIES][CUSTOM_HISTORY_BITS + 1];
static uint32_t customHistory;
static int32_t customScore;

static void
adjust_weight(int8_t *weight, int delta)
{
  if ((delta > 0 && *weight < INT8_MAX) ||
      (delta < 0 && *weight > INT8_MIN)) {
    *weight = (int8_t)(*weight + delta);
  }
}

static void
update_counter(uint8_t *counter, uint8_t outcome)
{
  if (outcome == TAKEN) {
    if (*counter < ST) {
      ++*counter;
    }
  } else if (*counter > SN) {
    --*counter;
  }
}


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void
init_predictor()
{
  if (bpType == CUSTOM) {
    customHistory = 0;
    customScore = 0;
    for (unsigned int row = 0; row < CUSTOM_ENTRIES; ++row) {
      customWeights[row][0] = -1; // Initially prefer NOTTAKEN.
      for (unsigned int i = 1; i <= CUSTOM_HISTORY_BITS; ++i) {
        customWeights[row][i] = 0;
      }
    }
  } else if (bpType == GSHARE) {
    // Validate the shift width before computing the table size.
    if (ghistoryBits < 0 || ghistoryBits >= 32) {
      fprintf(stderr, "Gshare history bits must be between 0 and 31.\n");
      exit(EXIT_FAILURE);
    }

    uint32_t tableSize = UINT32_C(1) << ghistoryBits;
    historyMask = tableSize - 1;
    globalHistory = 0;
    counterTable = malloc((size_t)tableSize * sizeof(*counterTable));
    if (counterTable == NULL) {
      fprintf(stderr, "Unable to allocate the Gshare counter table.\n");
      exit(EXIT_FAILURE);
    }

    // Start with not-taken history and weakly not-taken counters.
    for (uint32_t i = 0; i < tableSize; ++i) {
      counterTable[i] = WN;
    }
  } else if (bpType == TOURNAMENT) {
    if (ghistoryBits < 0 || ghistoryBits >= 32 ||
        lhistoryBits < 0 || lhistoryBits >= 32 ||
        pcIndexBits < 0 || pcIndexBits >= 32) {
      fprintf(stderr, "Tournament history and index bits must be between 0 and 31.\n");
      exit(EXIT_FAILURE);
    }

    uint32_t globalSize = UINT32_C(1) << ghistoryBits;
    uint32_t localSize = UINT32_C(1) << lhistoryBits;
    uint32_t pcSize = UINT32_C(1) << pcIndexBits;
    historyMask = globalSize - 1;
    localHistoryMask = localSize - 1;
    pcIndexMask = pcSize - 1;
    globalHistory = 0;

    counterTable = malloc((size_t)globalSize * sizeof(*counterTable));
    choiceTable = malloc((size_t)globalSize * sizeof(*choiceTable));
    localCounterTable = malloc((size_t)localSize * sizeof(*localCounterTable));
    localHistoryTable = calloc((size_t)pcSize, sizeof(*localHistoryTable));
    if (counterTable == NULL || choiceTable == NULL ||
        localCounterTable == NULL || localHistoryTable == NULL) {
      free(counterTable);
      free(choiceTable);
      free(localCounterTable);
      free(localHistoryTable);
      fprintf(stderr, "Unable to allocate the Tournament predictor tables.\n");
      exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < globalSize; ++i) {
      counterTable[i] = WN;
      choiceTable[i] = WN; // Weakly select the global predictor.
    }
    for (uint32_t i = 0; i < localSize; ++i) {
      localCounterTable[i] = WN;
    }
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{
  //
  //TODO: Implement prediction scheme
  //

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE: {
      uint32_t index = (pc ^ globalHistory) & historyMask;
      return counterTable[index] >= WT ? TAKEN : NOTTAKEN;
    }
    case TOURNAMENT: {
      uint32_t localHistory = localHistoryTable[pc & pcIndexMask];
      uint8_t counter = choiceTable[globalHistory] >= WT
                        ? localCounterTable[localHistory]
                        : counterTable[globalHistory];
      return counter >= WT ? TAKEN : NOTTAKEN;
    }
    case CUSTOM: {
      const int8_t *weights = customWeights[pc & (CUSTOM_ENTRIES - 1)];
      customScore = weights[0];
      for (unsigned int i = 0; i < CUSTOM_HISTORY_BITS; ++i) {
        customScore += (customHistory & (UINT32_C(1) << i))
                       ? weights[i + 1] : -weights[i + 1];
      }
      return customScore >= 0 ? TAKEN : NOTTAKEN;
    }
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void
train_predictor(uint32_t pc, uint8_t outcome)
{
  if (bpType == CUSTOM) {
    // Train with the same history and score used by make_prediction.
    int target = outcome == TAKEN ? 1 : -1;
    if ((customScore >= 0) != (outcome == TAKEN) ||
        (customScore >= -CUSTOM_THRESHOLD && customScore <= CUSTOM_THRESHOLD)) {
      int8_t *weights = customWeights[pc & (CUSTOM_ENTRIES - 1)];
      adjust_weight(&weights[0], target);
      for (unsigned int i = 0; i < CUSTOM_HISTORY_BITS; ++i) {
        int input = (customHistory & (UINT32_C(1) << i)) ? 1 : -1;
        adjust_weight(&weights[i + 1], target * input);
      }
    }
    customHistory = ((customHistory << 1) | outcome) & CUSTOM_HISTORY_MASK;
  } else if (bpType == GSHARE) {
    // Train the entry selected by the history used for prediction.
    uint32_t index = (pc ^ globalHistory) & historyMask;
    if (outcome == TAKEN) {
      if (counterTable[index] < ST) {
        ++counterTable[index];
      }
    } else {
      if (counterTable[index] > SN) {
        --counterTable[index];
      }
    }

    // Shift the actual outcome into the least significant bit.
    globalHistory = ((globalHistory << 1) | outcome) & historyMask;
  } else if (bpType == TOURNAMENT) {
    uint32_t pcIndex = pc & pcIndexMask;
    uint32_t localHistory = localHistoryTable[pcIndex];
    uint8_t globalPrediction = counterTable[globalHistory] >= WT;
    uint8_t localPrediction = localCounterTable[localHistory] >= WT;

    // Train the chooser only when one of the two predictors is correct.
    // Use both predictions from before any counters or histories change.
    if (globalPrediction != localPrediction) {
      update_counter(&choiceTable[globalHistory], localPrediction == outcome);
    }
    update_counter(&counterTable[globalHistory], outcome);
    update_counter(&localCounterTable[localHistory], outcome);

    localHistoryTable[pcIndex] = ((localHistory << 1) | outcome) & localHistoryMask;
    globalHistory = ((globalHistory << 1) | outcome) & historyMask;
  }
}
