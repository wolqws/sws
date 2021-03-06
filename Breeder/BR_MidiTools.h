/******************************************************************************
/ BR_MidiTools.h
/
/ Copyright (c) 2014 Dominik Martin Drzic
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
#pragma once

/******************************************************************************
* MIDI timebase - this is how Reaper stores timebase settings internally      *
******************************************************************************/
enum BR_MidiTimebase
{
	PROJECT_BEATS = 0,
	PROJECT_SYNC  = 1,
	PROJECT_TIME  = 2,
	SOURCE_BEATS  = 4
};

/******************************************************************************
* MIDI noteshow - this is how Reaper stores noteshow settings internally      *
******************************************************************************/
enum BR_MidiNoteshow
{
	SHOW_ALL_NOTES             = 0,
	HIDE_UNUSED_NOTES          = 1,
	HIDE_UNUSED_UNNAMED_NOTES  = 2
};

/******************************************************************************
* MIDI CC lanes - this is how Reaper stores CC lanes internally               *
******************************************************************************/
enum BR_MidiVelLanes
{
	CC_VELOCITY         = -1,
	CC_PITCH            = 128,
	CC_PROGRAM          = 129,
	CC_CHANNEL_PRESSURE = 130,
	CC_BANK_SELECT      = 131,
	CC_TEXT_EVENTS      = 132,
	CC_SYSEX            = 133,
	CC_14BIT_START    = 134
};

/******************************************************************************
* Class for managing normal or inline MIDI editor (read-only for now)         *
******************************************************************************/
class BR_MidiEditor
{
public:
	BR_MidiEditor (void* midiEditor);
	BR_MidiEditor (MediaItem_Take* take);

	/* Various MIDI editor view options */
	MediaItem_Take* GetActiveTake ();
	double GetStartPos ();            // can be ppq or time - depends on timebase
	double GetHZoom ();               // also dependent on timebase settings
	int GetVPos ();
	int GetVZoom ();
	int GetNoteshow ();               // see BR_MidiNoteshow
	int GetTimebase ();               // see BR_MidiTimeBase
	int GetPianoRoll ();
	int GetDrawChannel ();

	/* CC lanes */
	int CountCCLanes ();
	int GetCCLane (int idx);
	int GetCCLaneHeight (int idx);

	/* Check if MIDI editor data is valid - should call right after creating the object */
	bool IsValid ();

private:
	MediaItem_Take* m_take;
	void* m_midiEditor;
	double m_startPos, m_hZoom;
	int m_vPos, m_vZoom, m_noteshow, m_timebase, m_pianoroll, m_drawChannel, m_ccLanesCount;
	vector<int> m_ccLanes, m_ccLanesHeight;
	bool m_valid;
	bool Build ();
};

/******************************************************************************
* Class for saving and restoring TIME positioning info of an item and         *
* all of it's MIDI events                                                     *
******************************************************************************/
class BR_MidiItemTimePos
{
public:
	BR_MidiItemTimePos (MediaItem* item, bool deleteSavedEvents = true);
	void Restore (double offset = 0);

private:
	struct MidiTake
	{
		struct NoteEvent
		{
			NoteEvent (MediaItem_Take* take, int id);
			void InsertEvent (MediaItem_Take* take, double offset);
			bool selected, muted;
			double pos, end;
			int chan, pitch, vel;
		};
		struct CCEvent
		{
			CCEvent (MediaItem_Take* take, int id);
			void InsertEvent (MediaItem_Take* take, double offset);
			bool selected, muted;
			double pos;
			int chanmsg, chan, msg2, msg3;
		};
		struct SysEvent
		{
			SysEvent (MediaItem_Take* take, int id);
			void InsertEvent (MediaItem_Take* take, double offset);
			bool selected, muted;
			double pos;
			int type, msg_sz;
			WDL_TypedBuf<char> msg;
		};

		MidiTake (MediaItem_Take* take, int noteCount = 0, int ccCount = 0, int sysCount = 0);
		vector<NoteEvent> noteEvents;
		vector<CCEvent> ccEvents;
		vector<SysEvent> sysEvents;
		MediaItem_Take* take;
	};
	MediaItem* item;
	double position, length, timeBase;
	vector<BR_MidiItemTimePos::MidiTake> savedMidiTakes;
};

/******************************************************************************
* Mouse cursor                                                                *
******************************************************************************/
double ME_PositionAtMouseCursor (bool checkRuler, bool checkCCLanes);

/******************************************************************************
* Miscellaneous                                                               *
******************************************************************************/
vector<int> GetUsedNamedNotes (void* midiEditor, MediaItem_Take* take, bool used, bool named, int channelForNames);
vector<int> GetSelectedNotes (MediaItem_Take* take);
vector<int> MuteSelectedNotes (MediaItem_Take* take); // returns previous mute state of all notes
set<int> GetUsedCCLanes (MediaItem_Take* take, int dectect14bit); // dectect14bit: 0-> don't detect 14-bit, 1->detect partial 14-bit (if CC parts have additional data count them too) 2->detect full 14-bit (detect only if all CCs that make it have exactly same time positions)
double EffectiveMidiTakeLength (MediaItem_Take* take, bool ignoreMutedEvents, bool ignoreTextEvents);
double EffectiveMidiTakeStart (MediaItem_Take* take, bool ignoreMutedEvents, bool ignoreTextEvents);
void SetMutedNotes (MediaItem_Take* take, vector<int>& muteStatus);
void SetSelectedNotes (MediaItem_Take* take, vector<int>& selectedNotes, bool unselectOthers);
bool AreAllNotesUnselected (MediaItem_Take* take);
bool IsMidi (MediaItem_Take* take, bool* inProject = NULL);
bool IsOpenInInlineEditor (MediaItem_Take* take);
bool IsMidiNoteBlack (int note);
bool IsVelLaneValid (int lane);
int MapVelLaneToReaScriptCC (int lane); // CC format follows ReaScript scheme: 0-127=CC, 0x100|(0-31)=14-bit CC, 0x200=velocity, 0x201=pitch,
int MapReaScriptCCToVelLane (int cc);   // 0x202=program, 0x203=channel pressure, 0x204=bank/program select, 0x205=text, 0x206=sysex