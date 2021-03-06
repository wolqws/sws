/*****************************************************************************
/ BR.cpp
/
/ Copyright (c) 2012-2014 Dominik Martin Drzic
/ http://forum.cockos.com/member.php?u=27094
/ https://code.google.com/p/sws-extension
/
/ Permission is hereby granted, free of charge, to any person obtaining a copy
/ of this software and associated documentation files (the "Software"), to deal
/ in the Software without restriction, including without limitation the rights to
/ use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
/ of the Software, and to permit persons to whom the Software is furnished to
/ do so, subject to the following conditions:
/
/ The above copyright notice and this permission notice shall be included in all
/ copies or substantial portions of the Software.
/
/ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
/ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
/ OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
/ NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
/ HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
/ WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
/ OTHER DEALINGS IN THE SOFTWARE.
/
******************************************************************************/
#include "stdafx.h"
#include "BR.h"
#include "BR_ContinuousActions.h"
#include "BR_Envelope.h"
#include "BR_Loudness.h"
#include "BR_MidiEditor.h"
#include "BR_Misc.h"
#include "BR_ProjState.h"
#include "BR_Tempo.h"
#include "BR_Update.h"
#include "BR_Util.h"
#include "../reaper/localize.h"

//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions
static COMMAND_T g_commandTable[] =
{
	/******************************************************************************
	* Envelopes                                                                   *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Set closest envelope point's value to mouse cursor (perform until shortcut released)" },           "BR_ENV_PT_VAL_CLOSEST_MOUSE",      SetEnvPointMouseValue, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Set closest left side envelope point's value to mouse cursor (perform until shortcut released)" }, "BR_ENV_PT_VAL_CLOSEST_LEFT_MOUSE", SetEnvPointMouseValue, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Move edit cursor to next envelope point" },                                                        "SWS_BRMOVEEDITTONEXTENV",          CursorToEnv1, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Move edit cursor to next envelope point and select it" },                                          "SWS_BRMOVEEDITSELNEXTENV",         CursorToEnv1, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Move edit cursor to next envelope point and add to selection" },                                   "SWS_BRMOVEEDITTONEXTENVADDSELL",   CursorToEnv2, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Move edit cursor to previous envelope point" },                                                    "SWS_BRMOVEEDITTOPREVENV",          CursorToEnv1, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Move edit cursor to previous envelope point and select it" },                                      "SWS_BRMOVEEDITSELPREVENV",         CursorToEnv1, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Move edit cursor to previous envelope point and add to selection" },                               "SWS_BRMOVEEDITTOPREVENVADDSELL",   CursorToEnv2, NULL, -1},

	{ { DEFACCEL, "SWS/BR: Select next envelope point" },                                                                     "BR_ENV_SEL_NEXT_POINT",            SelNextPrevEnvPoint, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Select previous envelope point" },                                                                 "BR_ENV_SEL_PREV_POINT",            SelNextPrevEnvPoint, NULL, -1},

	{ { DEFACCEL, "SWS/BR: Expand envelope point selection to the right" },                                                   "BR_ENV_SEL_EXPAND_RIGHT",          ExpandEnvSel,    NULL, 1},
	{ { DEFACCEL, "SWS/BR: Expand envelope point selection to the left" },                                                    "BR_ENV_SEL_EXPAND_LEFT",           ExpandEnvSel,    NULL, -1},
	{ { DEFACCEL, "SWS/BR: Expand envelope point selection to the right (end point only)" },                                  "BR_ENV_SEL_EXPAND_RIGHT_END",      ExpandEnvSelEnd, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Expand envelope point selection to the left (end point only)" },                                   "BR_ENV_SEL_EXPAND_L_END",          ExpandEnvSelEnd, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Shrink envelope point selection from the right" },                                                 "BR_ENV_SEL_SHRINK_RIGHT",          ShrinkEnvSel,    NULL, 1},
	{ { DEFACCEL, "SWS/BR: Shrink envelope point selection from the left" },                                                  "BR_ENV_SEL_SHRINK_LEFT",           ShrinkEnvSel,    NULL, -1},
	{ { DEFACCEL, "SWS/BR: Shrink envelope point selection from the right (end point only)" },                                "BR_ENV_SEL_SHRINK_RIGHT_END",      ShrinkEnvSelEnd, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Shrink envelope point selection from the left (end point only)" },                                 "BR_ENV_SEL_SHRINK_LEFT_END",       ShrinkEnvSelEnd, NULL, -1},

	{ { DEFACCEL, "SWS/BR: Shift envelope point selection left" },                                                            "BR_ENV_SHIFT_SEL_LEFT",            ShiftEnvSelection, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Shift envelope point selection right" },                                                           "BR_ENV_SHIFT_SEL_RIGHT",           ShiftEnvSelection, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Select peaks in envelope (add to selection)" },                                                    "BR_ENV_SEL_PEAKS_ADD",             PeaksDipsEnv, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Select peaks in envelope" },                                                                       "BR_ENV_SEL_PEAKS",                 PeaksDipsEnv, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Select dips in envelope (add to selection)" },                                                     "BR_ENV_SEL_DIPS_ADD",              PeaksDipsEnv, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Select dips in envelope" },                                                                        "BR_ENV_SEL_DIPS",                  PeaksDipsEnv, NULL, -2},

	{ { DEFACCEL, "SWS/BR: Unselect envelope points outside time selection" },                                                "BR_ENV_UNSEL_OUT_TIME_SEL",        SelEnvTimeSel, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Unselect envelope points in time selection" },                                                     "BR_ENV_UNSEL_IN_TIME_SEL",         SelEnvTimeSel, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Set selected envelope points to next point's value" },                                             "BR_SET_ENV_TO_NEXT_VAL",           SetEnvValToNextPrev, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Set selected envelope points to previous point's value" },                                         "BR_SET_ENV_TO_PREV_VAL",           SetEnvValToNextPrev, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Set selected envelope points to last selected point's value" },                                    "BR_SET_ENV_TO_LAST_SEL_VAL",       SetEnvValToNextPrev, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Set selected envelope points to first selected point's value" },                                   "BR_SET_ENV_TO_FIRST_SEL_VAL",      SetEnvValToNextPrev, NULL, -2},

	{ { DEFACCEL, "SWS/BR: Move closest envelope point to edit cursor" },                                                     "BR_MOVE_CLOSEST_ENV_ECURSOR",      MoveEnvPointToEditCursor, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Move closest selected envelope point to edit cursor" },                                            "BR_MOVE_CLOSEST_SEL_ENV_ECURSOR",  MoveEnvPointToEditCursor, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Insert 2 envelope points at time selection" },                                                     "BR_INSERT_2_ENV_POINT_TIME_SEL",   Insert2EnvPointsTimeSelection, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Fit selected envelope points to time selection" },                                                 "BR_FIT_ENV_POINTS_TO_TIMESEL",     FitEnvPointsToTimeSel, NULL},

	{ { DEFACCEL, "SWS/BR: Hide all but selected track envelope for all tracks" },                                            "BR_ENV_HIDE_ALL_BUT_ACTIVE",       ShowActiveTrackEnvOnly, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Hide all but selected track envelope for selected tracks" },                                       "BR_ENV_HIDE_ALL_BUT_ACTIVE_SEL",   ShowActiveTrackEnvOnly, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Insert new envelope point at mouse cursor using value at current position (obey snapping)" },      "BR_ENV_POINT_MOUSE_CURSOR",        CreateEnvPointMouse, NULL},

	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 1" },                                                          "BR_SAVE_ENV_SEL_SLOT_1",           SaveEnvSelSlot, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 2" },                                                          "BR_SAVE_ENV_SEL_SLOT_2",           SaveEnvSelSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 3" },                                                          "BR_SAVE_ENV_SEL_SLOT_3",           SaveEnvSelSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 4" },                                                          "BR_SAVE_ENV_SEL_SLOT_4",           SaveEnvSelSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 5" },                                                          "BR_SAVE_ENV_SEL_SLOT_5",           SaveEnvSelSlot, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 1" },                                                       "BR_RESTORE_ENV_SEL_SLOT_1",        RestoreEnvSelSlot, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 2" },                                                       "BR_RESTORE_ENV_SEL_SLOT_2",        RestoreEnvSelSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 3" },                                                       "BR_RESTORE_ENV_SEL_SLOT_3",        RestoreEnvSelSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 4" },                                                       "BR_RESTORE_ENV_SEL_SLOT_4",        RestoreEnvSelSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 5" },                                                       "BR_RESTORE_ENV_SEL_SLOT_5",        RestoreEnvSelSlot, NULL, 4},

	/******************************************************************************
	* Loudness                                                                    *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Analyze loudness..." },                               "BR_ANALAYZE_LOUDNESS_DLG",       AnalyzeLoudness, NULL, 0, IsAnalyzeLoudnessVisible},
	{ { DEFACCEL, "SWS/BR: Normalize loudness of selected tracks..." },          "BR_NORMALIZE_LOUDNESS_TRACKS",   NormalizeLoudness, NULL, 0, },
	{ { DEFACCEL, "SWS/BR: Normalize loudness of selected tracks to -23 LUFS" }, "BR_NORMALIZE_LOUDNESS_TRACKS23", NormalizeLoudness, NULL, 1, },
	{ { DEFACCEL, "SWS/BR: Normalize loudness of selected items..." },           "BR_NORMALIZE_LOUDNESS_ITEMS",    NormalizeLoudness, NULL, 2, },
	{ { DEFACCEL, "SWS/BR: Normalize loudness of selected items to -23 LUFS" },  "BR_NORMALIZE_LOUDNESS_ITEMS23",  NormalizeLoudness, NULL, 3, },

	/******************************************************************************
	* Midi editor - Media item preview                                            *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Stop media item preview" },                                                                                                   "BR_ME_STOP_PREV_ACT_ITEM",             NULL, NULL, 0,    NULL, 32060, ME_StopMidiTakePreview},

	{ { DEFACCEL, "SWS/BR: Preview active media item through track" },                                                                                   "BR_ME_PREV_ACT_ITEM",                  NULL, NULL, 1111, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item through track (start from mouse position)" },                                                       "BR_ME_PREV_ACT_ITEM_POS",              NULL, NULL, 1211, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item through track (sync with next measure)" },                                                          "BR_ME_PREV_ACT_ITEM_SYNC",             NULL, NULL, 1311, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item through track and pause during preview" },                                                          "BR_ME_PREV_ACT_ITEM_PAUSE",            NULL, NULL, 1112, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item through track and pause during preview (start from mouse position)" },                              "BR_ME_PREV_ACT_ITEM_PAUSE_POS",        NULL, NULL, 1212, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item (selected notes only) through track" },                                                             "BR_ME_PREV_ACT_ITEM_NOTES",            NULL, NULL, 1121, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item (selected notes only) through track (start from mouse position)" },                                 "BR_ME_PREV_ACT_ITEM_NOTES_POS",        NULL, NULL, 1221, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item (selected notes only) through track (sync with next measure)" },                                    "BR_ME_PREV_ACT_ITEM_NOTES_SYNC",       NULL, NULL, 1321, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item (selected notes only) through track and pause during preview" },                                    "BR_ME_PREV_ACT_ITEM_NOTES_PAUSE",      NULL, NULL, 1122, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item (selected notes only) through track and pause during preview (start from mouse position)" },        "BR_ME_PREV_ACT_ITEM_NOTES_PAUSE_POS",  NULL, NULL, 1222, NULL, 32060, ME_PreviewActiveTake},

	{ { DEFACCEL, "SWS/BR: Toggle preview active media item through track" },                                                                            "BR_ME_TPREV_ACT_ITEM",                 NULL, NULL, 2111, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item through track (start from mouse position)" },                                                "BR_ME_TPREV_ACT_ITEM_POS",             NULL, NULL, 2211, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item through track (sync with next measure)" },                                                   "BR_ME_TPREV_ACT_ITEM_SYNC",            NULL, NULL, 2311, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item through track and pause during preview" },                                                   "BR_ME_TPREV_ACT_ITEM_PAUSE",           NULL, NULL, 2112, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item through track and pause during preview (start from mouse position)" },                       "BR_ME_TPREV_ACT_ITEM_PAUSE_POS",       NULL, NULL, 2212, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item (selected notes only) through track" },                                                      "BR_ME_TPREV_ACT_ITEM_NOTES",           NULL, NULL, 2121, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item (selected notes only) through track (start from mouse position)" },                          "BR_ME_TPREV_ACT_ITEM_NOTES_POS",       NULL, NULL, 2221, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item (selected notes only) through track (sync with next measure)" },                             "BR_ME_TPREV_ACT_ITEM_NOTES_SYNC",      NULL, NULL, 2321, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item (selected notes only) through track and pause during preview" },                             "BR_ME_TPREV_ACT_ITEM_NOTES_PAUSE",     NULL, NULL, 2122, NULL, 32060, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item (selected notes only) through track and pause during preview (start from mouse position)" }, "BR_ME_TPREV_ACT_ITEM_NOTES_PAUSE_POS", NULL, NULL, 2222, NULL, 32060, ME_PreviewActiveTake},

	/******************************************************************************
	* Midi editor - Misc                                                          *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Play from mouse cursor position" },               "BR_ME_PLAY_MOUSECURSOR",          NULL, NULL, 0, NULL, 32060, ME_PlaybackAtMouseCursor},
	{ { DEFACCEL, "SWS/BR: Play/pause from mouse cursor position" },         "BR_ME_PLAY_PAUSE_MOUSECURSOR",    NULL, NULL, 1, NULL, 32060, ME_PlaybackAtMouseCursor},
	{ { DEFACCEL, "SWS/BR: Play/stop from mouse cursor position" },          "BR_ME_PLAY_STOP_MOUSECURSOR",     NULL, NULL, 2, NULL, 32060, ME_PlaybackAtMouseCursor},

	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 1" },             "BR_ME_SAVE_CURSOR_POS_SLOT_1",    NULL, NULL, 0, NULL, 32060, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 2" },             "BR_ME_SAVE_CURSOR_POS_SLOT_2",    NULL, NULL, 1, NULL, 32060, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 3" },             "BR_ME_SAVE_CURSOR_POS_SLOT_3",    NULL, NULL, 2, NULL, 32060, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 4" },             "BR_ME_SAVE_CURSOR_POS_SLOT_4",    NULL, NULL, 3, NULL, 32060, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 5" },             "BR_ME_SAVE_CURSOR_POS_SLOT_5",    NULL, NULL, 4, NULL, 32060, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 1" },          "BR_ME_RESTORE_CURSOR_POS_SLOT_1", NULL, NULL, 0, NULL, 32060, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 2" },          "BR_ME_RESTORE_CURSOR_POS_SLOT_2", NULL, NULL, 1, NULL, 32060, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 3" },          "BR_ME_RESTORE_CURSOR_POS_SLOT_3", NULL, NULL, 2, NULL, 32060, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 4" },          "BR_ME_RESTORE_CURSOR_POS_SLOT_4", NULL, NULL, 3, NULL, 32060, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 5" },          "BR_ME_RESTORE_CURSOR_POS_SLOT_5", NULL, NULL, 4, NULL, 32060, ME_RestoreCursorPosSlot},

	{ { DEFACCEL, "SWS/BR: Save note selection from active take, slot 1" },  "BR_ME_SAVE_NOTE_SEL_SLOT_1",      NULL, NULL, 0, NULL, 32060, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active take, slot 2" },  "BR_ME_SAVE_NOTE_SEL_SLOT_2",      NULL, NULL, 1, NULL, 32060, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active take, slot 3" },  "BR_ME_SAVE_NOTE_SEL_SLOT_3",      NULL, NULL, 2, NULL, 32060, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active take, slot 4" },  "BR_ME_SAVE_NOTE_SEL_SLOT_4",      NULL, NULL, 3, NULL, 32060, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active take, slot 5" },  "BR_ME_SAVE_NOTE_SEL_SLOT_5",      NULL, NULL, 4, NULL, 32060, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active take, slot 1" }, "BR_ME_RESTORE_NOTE_SEL_SLOT_1",   NULL, NULL, 0, NULL, 32060, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active take, slot 2" }, "BR_ME_RESTORE_NOTE_SEL_SLOT_2",   NULL, NULL, 1, NULL, 32060, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active take, slot 3" }, "BR_ME_RESTORE_NOTE_SEL_SLOT_3",   NULL, NULL, 2, NULL, 32060, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active take, slot 4" }, "BR_ME_RESTORE_NOTE_SEL_SLOT_4",   NULL, NULL, 3, NULL, 32060, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active take, slot 5" }, "BR_ME_RESTORE_NOTE_SEL_SLOT_5",   NULL, NULL, 4, NULL, 32060, ME_RestoreNoteSelSlot},

	{ { DEFACCEL, "SWS/BR: Show only used CC lanes (detect 14-bit)" },       "BR_ME_SHOW_USED_CC_14_BIT",       NULL, NULL, 0, NULL, 32060, ME_ShowUsedCCLanesDetect14Bit},

	/******************************************************************************
	* Misc                                                                        *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Split selected items at tempo markers" },                                               "SWS_BRSPLITSELECTEDTEMPO",         SplitItemAtTempo},
	{ { DEFACCEL, "SWS/BR: Create project markers from selected tempo markers" },                                  "BR_TEMPO_TO_MARKERS",              MarkersAtTempo},
	{ { DEFACCEL, "SWS/BR: Enable \"Ignore project tempo\" for selected MIDI items (use tempo at item's start)" }, "BR_MIDI_PROJ_TEMPO_ENB",           MidiItemTempo, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Disable \"Ignore project tempo\" for selected MIDI items" },                            "BR_MIDI_PROJ_TEMPO_DIS",           MidiItemTempo, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Trim MIDI item to active content" },                                                    "BR_TRIM_MIDI_ITEM_ACT_CONTENT",    MidiItemTrim, NULL},

	{ { DEFACCEL, "SWS/BR: Create project markers from notes in selected MIDI items" },                            "BR_MIDI_NOTES_TO_MARKERS",         MarkersAtNotes},
	{ { DEFACCEL, "SWS/BR: Create project markers from stretch markers in selected items" },                       "BR_STRETCH_MARKERS_TO_MARKERS",    MarkersAtStretchMarkers},
	{ { DEFACCEL, "SWS/BR: Create project markers from selected items (name by item's notes)" },                   "BR_ITEMS_TO_MARKERS_NOTES",        MarkersRegionsAtItems, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Create regions from selected items (name by item's notes)" },                           "BR_ITEMS_TO_REGIONS_NOTES",        MarkersRegionsAtItems, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Toggle \"Grid snap settings follow grid visibility\"" },                                "BR_OPTIONS_SNAP_FOLLOW_GRID_VIS",  SnapFollowsGridVis, NULL, 0, IsSnapFollowsGridVisOn},
	{ { DEFACCEL, "SWS/BR: Toggle \"Playback position follows project timebase when changing tempo\"" },           "BR_OPTIONS_PLAYBACK_TEMPO_CHANGE", PlaybackFollowsTempoChange, NULL, 0, IsPlaybackFollowingTempoChange},
	{ { DEFACCEL, "SWS/BR: Set \"Apply trim when adding volume/pan envelopes\" to \"Always\"" },                   "BR_OPTIONS_ENV_TRIM_ALWAYS",       TrimNewVolPanEnvs, NULL, 0, IsTrimNewVolPanEnvsOn},
	{ { DEFACCEL, "SWS/BR: Set \"Apply trim when adding volume/pan envelopes\" to \"In read/write\"" },            "BR_OPTIONS_ENV_TRIM_READWRITE",    TrimNewVolPanEnvs, NULL, 1, IsTrimNewVolPanEnvsOn},
	{ { DEFACCEL, "SWS/BR: Set \"Apply trim when adding volume/pan envelopes\" to \"Never\"" },                    "BR_OPTIONS_ENV_TRIM_NEVER",        TrimNewVolPanEnvs, NULL, 2, IsTrimNewVolPanEnvsOn},

	{ { DEFACCEL, "SWS/BR: Cycle through record modes" },                                                          "BR_CYCLE_RECORD_MODES",            CycleRecordModes},
	{ { DEFACCEL, "SWS/BR: Focus arrange window" },                                                                "BR_FOCUS_ARRANGE_WND",             FocusArrange},
	{ { DEFACCEL, "SWS/BR: Toggle media item online/offline" },                                                    "BR_TOGGLE_ITEM_ONLINE",            ToggleItemOnline},
	{ { DEFACCEL, "SWS/BR: Copy take media source file path of selected items to clipboard" },                     "BR_TSOURCE_PATH_TO_CLIPBOARD",     ItemSourcePathToClipBoard},

	{ { DEFACCEL, "SWS/BR: Play from mouse cursor position" },                                                     "BR_PLAY_MOUSECURSOR",              PlaybackAtMouseCursor, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Play/pause from mouse cursor position" },                                               "BR_PLAY_PAUSE_MOUSECURSOR",        PlaybackAtMouseCursor, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Play/stop from mouse cursor position" },                                                "BR_PLAY_STOP_MOUSECURSOR",         PlaybackAtMouseCursor, NULL, 2},

	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 1" },                                                   "BR_SAVE_CURSOR_POS_SLOT_1",        SaveCursorPosSlot, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 2" },                                                   "BR_SAVE_CURSOR_POS_SLOT_2",        SaveCursorPosSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 3" },                                                   "BR_SAVE_CURSOR_POS_SLOT_3",        SaveCursorPosSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 4" },                                                   "BR_SAVE_CURSOR_POS_SLOT_4",        SaveCursorPosSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 5" },                                                   "BR_SAVE_CURSOR_POS_SLOT_5",        SaveCursorPosSlot, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 1" },                                                "BR_RESTORE_CURSOR_POS_SLOT_1",     RestoreCursorPosSlot, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 2" },                                                "BR_RESTORE_CURSOR_POS_SLOT_2",     RestoreCursorPosSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 3" },                                                "BR_RESTORE_CURSOR_POS_SLOT_3",     RestoreCursorPosSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 4" },                                                "BR_RESTORE_CURSOR_POS_SLOT_4",     RestoreCursorPosSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 5" },                                                "BR_RESTORE_CURSOR_POS_SLOT_5",     RestoreCursorPosSlot, NULL, 4},

	/******************************************************************************
	* Media item preview                                                          *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse" },                                                                                          "BR_PREV_ITEM_CURSOR",                 PreviewItemAtMouse, NULL, 1111},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse (start from mouse cursor position)" },                                                       "BR_PREV_ITEM_CURSOR_POS",             PreviewItemAtMouse, NULL, 1121},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse (sync with next measure)" },                                                                 "BR_PREV_ITEM_CURSOR_SYNC",            PreviewItemAtMouse, NULL, 1131},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse and pause during preview" },                                                                 "BR_PREV_ITEM_PAUSE_CURSOR",           PreviewItemAtMouse, NULL, 1112},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse and pause during preview (start from mouse cursor position)" },                              "BR_PREV_ITEM_PAUSE_CURSOR_POS",       PreviewItemAtMouse, NULL, 1122},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse at track fader volume" },                                                                    "BR_PREV_ITEM_CURSOR_FADER",           PreviewItemAtMouse, NULL, 1211},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse at track fader volume (start from mouse position)" },                                        "BR_PREV_ITEM_CURSOR_FADER_POS",       PreviewItemAtMouse, NULL, 1221},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse at track fader volume (sync with next measure)" },                                           "BR_PREV_ITEM_CURSOR_FADER_SYNC",      PreviewItemAtMouse, NULL, 1231},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse at track fader volume and pause during preview" },                                           "BR_PREV_ITEM_PAUSE_CURSOR_FADER",     PreviewItemAtMouse, NULL, 1212},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse at track fader volume and pause during preview (start from mouse cursor position)" },        "BR_PREV_ITEM_PAUSE_CURSOR_FADER_POS", PreviewItemAtMouse, NULL, 1222},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse through track" },                                                                            "BR_PREV_ITEM_CURSOR_TRACK",           PreviewItemAtMouse, NULL, 1311},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse through track (start from mouse position)" },                                                "BR_PREV_ITEM_CURSOR_TRACK_POS",       PreviewItemAtMouse, NULL, 1321},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse through track (sync with next measure)" },                                                   "BR_PREV_ITEM_CURSOR_TRACK_SYNC",      PreviewItemAtMouse, NULL, 1331},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse through track and pause during preview" },                                                   "BR_PREV_ITEM_PAUSE_CURSOR_TRACK",     PreviewItemAtMouse, NULL, 1312},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse through track and pause during preview (start from mouse cursor position)" },                "BR_PREV_ITEM_PAUSE_CURSOR_TRACK_POS", PreviewItemAtMouse, NULL, 1322},

	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse" },                                                                                   "BR_TPREV_ITEM_CURSOR",                 PreviewItemAtMouse, NULL, 2111},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse (start from mouse position)" },                                                       "BR_TPREV_ITEM_CURSOR_POS",             PreviewItemAtMouse, NULL, 2121},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse (sync with next measure)" },                                                          "BR_TPREV_ITEM_CURSOR_SYNC",            PreviewItemAtMouse, NULL, 2131},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse and pause during preview" },                                                          "BR_TPREV_ITEM_PAUSE_CURSOR",           PreviewItemAtMouse, NULL, 2112},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse and pause during preview (start from mouse cursor position)" },                       "BR_TPREV_ITEM_PAUSE_CURSOR_POS",       PreviewItemAtMouse, NULL, 2122},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse at track fader volume" },                                                             "BR_TPREV_ITEM_CURSOR_FADER",           PreviewItemAtMouse, NULL, 2211},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse at track fader volume (start from mouse position)" },                                 "BR_TPREV_ITEM_CURSOR_FADER_POS",       PreviewItemAtMouse, NULL, 2221},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse at track fader volume (sync with next measure)" },                                    "BR_TPREV_ITEM_CURSOR_FADER_SYNC",      PreviewItemAtMouse, NULL, 2231},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse at track fader volume and pause during preview" },                                    "BR_TPREV_ITEM_PAUSE_CURSOR_FADER",     PreviewItemAtMouse, NULL, 2212},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse at track fader volume and pause during preview (start from mouse cursor position)" }, "BR_TPREV_ITEM_PAUSE_CURSOR_FADER_POS", PreviewItemAtMouse, NULL, 2222},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse through track" },                                                                     "BR_TPREV_ITEM_CURSOR_TRACK",           PreviewItemAtMouse, NULL, 2311},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse through track (start from mouse position)" },                                         "BR_TPREV_ITEM_CURSOR_TRACK_POS",       PreviewItemAtMouse, NULL, 2321},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse through track (sync with next measure)" },                                            "BR_TPREV_ITEM_CURSOR_TRACK_SYNC",      PreviewItemAtMouse, NULL, 2331},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse through track and pause during preview" },                                            "BR_TPREV_ITEM_PAUSE_CURSOR_TRACK",     PreviewItemAtMouse, NULL, 2312},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse through track and pause during preview (start from mouse cursor position)" },         "BR_TPREV_ITEM_PAUSE_CURSOR_TRACK_POS", PreviewItemAtMouse, NULL, 2322},

	/******************************************************************************
	* Grid                                                                        *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Move closest tempo marker to mouse cursor (perform until shortcut released)" },        "BR_MOVE_CLOSEST_TEMPO_MOUSE", MoveGridToMouse, NULL, 0},

	{ { DEFACCEL, "SWS/BR: Move closest grid line to mouse cursor (perform until shortcut released)" },           "BR_MOVE_GRID_TO_MOUSE",       MoveGridToMouse, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Move closest measure grid line to mouse cursor (perform until shortcut released)" },   "BR_MOVE_M_GRID_TO_MOUSE",     MoveGridToMouse, NULL, 2},

	{ { DEFACCEL, "SWS/BR: Move closest grid line to edit cursor" },                                              "BR_MOVE_GRID_TO_EDIT_CUR",    MoveGridToEditPlayCursor, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Move closest grid line to play cursor" },                                              "BR_MOVE_GRID_TO_PLAY_CUR",    MoveGridToEditPlayCursor, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Move closest measure grid line to edit cursor" },                                      "BR_MOVE_M_GRID_TO_EDIT_CUR",  MoveGridToEditPlayCursor, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Move closest measure grid line to play cursor" },                                      "BR_MOVE_M_GRID_TO_PLAY_CUR",  MoveGridToEditPlayCursor, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Move closest left side grid line to edit cursor" },                                    "BR_MOVE_L_GRID_TO_EDIT_CUR",  MoveGridToEditPlayCursor, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Move closest right side grid line to edit cursor" },                                   "BR_MOVE_R_GRID_TO_EDIT_CUR",  MoveGridToEditPlayCursor, NULL, 5},

	/******************************************************************************
	* Tempo                                                                       *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Move tempo marker forward 0.1 ms" },                                                               "SWS_BRMOVETEMPOFORWARD01",    MoveTempo, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Move tempo marker forward 1 ms" },                                                                 "SWS_BRMOVETEMPOFORWARD1",     MoveTempo, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Move tempo marker forward 10 ms"},                                                                 "SWS_BRMOVETEMPOFORWARD10",    MoveTempo, NULL, 100},
	{ { DEFACCEL, "SWS/BR: Move tempo marker forward 100 ms"},                                                                "SWS_BRMOVETEMPOFORWARD100",   MoveTempo, NULL, 1000},
	{ { DEFACCEL, "SWS/BR: Move tempo marker forward 1000 ms" },                                                              "SWS_BRMOVETEMPOFORWARD1000",  MoveTempo, NULL, 10000},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back 0.1 ms"},                                                                   "SWS_BRMOVETEMPOBACK01",       MoveTempo, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back 1 ms"},                                                                     "SWS_BRMOVETEMPOBACK1",        MoveTempo, NULL, -10},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back 10 ms"},                                                                    "SWS_BRMOVETEMPOBACK10",       MoveTempo, NULL, -100},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back 100 ms"},                                                                   "SWS_BRMOVETEMPOBACK100",      MoveTempo, NULL, -1000},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back 1000 ms"},                                                                  "SWS_BRMOVETEMPOBACK1000",     MoveTempo, NULL, -10000},
	{ { DEFACCEL, "SWS/BR: Move tempo marker forward" },                                                                      "SWS_BRMOVETEMPOFORWARD",      MoveTempo, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back" },                                                                         "SWS_BRMOVETEMPOBACK",         MoveTempo, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Move closest tempo marker to edit cursor" },                                                       "BR_MOVE_CLOSEST_TEMPO",       MoveTempo, NULL, 3},

	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.001 BPM (preserve overall tempo)" },                                       "BR_INC_TEMPO_0.001_BPM",      EditTempo, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.01 BPM (preserve overall tempo)" },                                        "BR_INC_TEMPO_0.01_BPM",       EditTempo, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.1 BPM (preserve overall tempo)" },                                         "BR_INC_TEMPO_0.1_BPM",        EditTempo, NULL, 100},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 01 BPM (preserve overall tempo)" },                                          "BR_INC_TEMPO_1_BPM",          EditTempo, NULL, 1000},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.001 BPM (preserve overall tempo)" },                                       "BR_DEC_TEMPO_0.001_BPM",      EditTempo, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.01 BPM (preserve overall tempo)" },                                        "BR_DEC_TEMPO_0.01_BPM",       EditTempo, NULL, -10},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.1 BPM (preserve overall tempo)" },                                         "BR_DEC_TEMPO_0.1_BPM",        EditTempo, NULL, -100},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 01 BPM (preserve overall tempo)" },                                          "BR_DEC_TEMPO_1_BPM",          EditTempo, NULL, -1000},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.001% (preserve overall tempo)" },                                          "BR_INC_TEMPO_0.001_PERC",     EditTempo, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.01% (preserve overall tempo)" },                                           "BR_INC_TEMPO_0.01_PERC",      EditTempo, NULL, 20},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.1% (preserve overall tempo)" },                                            "BR_INC_TEMPO_0.1_PERC",       EditTempo, NULL, 200},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 01% (preserve overall tempo)" },                                             "BR_INC_TEMPO_1_PERC",         EditTempo, NULL, 2000},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.001% (preserve overall tempo)" },                                          "BR_DEC_TEMPO_0.001_PERC",     EditTempo, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.01% (preserve overall tempo)" },                                           "BR_DEC_TEMPO_0.01_PERC",      EditTempo, NULL, -20},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.1% (preserve overall tempo)" },                                            "BR_DEC_TEMPO_0.1_PERC",       EditTempo, NULL, -200},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 01% (preserve overall tempo)" },                                             "BR_DEC_TEMPO_1_PERC",         EditTempo, NULL, -2000},

	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.001 BPM)" },                                       "BR_INC_GR_TEMPO_0.001_BPM",   EditTempoGradual, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.01 BPM)" },                                        "BR_INC_GR_TEMPO_0.01_BPM",    EditTempoGradual, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.1 BPM)" },                                         "BR_INC_GR_TEMPO_0.1_BPM",     EditTempoGradual, NULL, 100},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 01 BPM)" },                                          "BR_INC_GR_TEMPO_1_BPM",       EditTempoGradual, NULL, 1000},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.001 BPM)" },                                       "BR_DEC_GR_TEMPO_0.001_BPM",   EditTempoGradual, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.01 BPM)" },                                        "BR_DEC_GR_TEMPO_0.01_BPM",    EditTempoGradual, NULL, -10},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.1 BPM)" },                                         "BR_DEC_GR_TEMPO_0.1_BPM",     EditTempoGradual, NULL, -100},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 01 BPM)" },                                          "BR_DEC_GR_TEMPO_1_BPM",       EditTempoGradual, NULL, -1000},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.001%)" },                                          "BR_INC_GR_TEMPO_0.001_PERC",  EditTempoGradual, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.01%)" },                                           "BR_INC_GR_TEMPO_0.01_PERC",   EditTempoGradual, NULL, 20},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.1%)" },                                            "BR_INC_GR_TEMPO_0.1_PERC",    EditTempoGradual, NULL, 200},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 01%)" },                                             "BR_INC_GR_TEMPO_1_PERC",      EditTempoGradual, NULL, 2000},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.001%)" },                                          "BR_DEC_GR_TEMPO_0.001_PERC",  EditTempoGradual, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.01%)" },                                           "BR_DEC_GR_TEMPO_0.01_PERC",   EditTempoGradual, NULL, -20},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.1%)" },                                            "BR_DEC_GR_TEMPO_0.1_PERC",    EditTempoGradual, NULL, -200},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 01%)" },                                             "BR_DEC_GR_TEMPO_1_PERC",      EditTempoGradual, NULL, -2000},

	{ { DEFACCEL, "SWS/BR: Delete tempo marker (preserve overall tempo and positions if possible)" },                         "BR_DELETE_TEMPO",             DeleteTempo},
	{ { DEFACCEL, "SWS/BR: Delete tempo marker and preserve position and length of items (including MIDI events)" },          "BR_DELETE_TEMPO_ITEMS",       DeleteTempoPreserveItems, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Delete tempo marker and preserve position and length of selected items (including MIDI events)" }, "BR_DELETE_TEMPO_ITEMS_SEL",   DeleteTempoPreserveItems, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Create tempo markers at grid after every selected tempo marker" },                                 "BR_TEMPO_GRID",               TempoAtGrid},

	{ { DEFACCEL, "SWS/BR: Convert project markers to tempo markers..." },                                                    "SWS_BRCONVERTMARKERSTOTEMPO", ConvertMarkersToTempoDialog, NULL, 0, IsConvertMarkersToTempoVisible},
	{ { DEFACCEL, "SWS/BR: Select and adjust tempo markers..." },                                                             "SWS_BRADJUSTSELTEMPO",        SelectAdjustTempoDialog, NULL, 0, IsSelectAdjustTempoVisible},
	{ { DEFACCEL, "SWS/BR: Randomize tempo markers..." },                                                                     "BR_RANDOMIZE_TEMPO",          RandomizeTempoDialog},

	{ { DEFACCEL, "SWS/BR: Set tempo marker shape (options)..." },                                                            "BR_TEMPO_SHAPE_OPTIONS",      TempoShapeOptionsDialog, NULL, 0, IsTempoShapeOptionsVisible},
	{ { DEFACCEL, "SWS/BR: Set tempo marker shape to linear (preserve positions)" },                                          "BR_TEMPO_SHAPE_LINEAR",       TempoShapeLinear},
	{ { DEFACCEL, "SWS/BR: Set tempo marker shape to square (preserve positions)" },                                          "BR_TEMPO_SHAPE_SQUARE",       TempoShapeSquare},

	{ {}, LAST_COMMAND, },
};
//!WANT_LOCALIZE_1ST_STRING_END

static void InitContinuousActions ()
{
	MoveGridToMouseInit();
	SetEnvPointMouseValueInit();
}

int BR_Init ()
{
	SWSRegisterCommands(g_commandTable);
	InitContinuousActions (); // call only after registering all actions
	ProjStateInit();
	AnalyzeLoudnessInit();
	VersionCheckInit();
	return 1;
}

void BR_Exit ()
{
	AnalyzeLoudnessExit();
}

bool BR_ActionHook (int cmd, int flag)
{
	return ContinuousActionHook(cmd, flag);
}

void BR_CSurfSetPlayState (bool play, bool pause, bool rec)
{
	MidiTakePreviewPlayState(play, rec);
}