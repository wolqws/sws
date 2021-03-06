/******************************************************************************
/ BR_Tempo.cpp
/
/ Copyright (c) 2013 Dominik Martin Drzic
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
#include "BR_Tempo.h"
#include "BR_ContinuousActions.h"
#include "BR_EnvTools.h"
#include "BR_MidiTools.h"
#include "BR_TempoDlg.h"
#include "BR_Util.h"
#include "../SnM/SnM_Util.h"
#include "../reaper/localize.h"

/******************************************************************************
* Globals                                                                     *
******************************************************************************/
static bool g_convertMarkersToTempoDialog = false;
static bool g_selectAdjustTempoDialog = false;
static bool g_tempoShapeDialog = false;

/******************************************************************************
* Continuous action: move grid to mouse cursor                                *
******************************************************************************/
static BR_Envelope* g_moveGridTempoMap = NULL;
static bool         g_movedGridOnce    = false;

static bool MoveGridInit (bool init)
{
	static int s_editCursorUndo = 0;

	bool initSuccessful = true;
	if (init)
	{
		GetConfig("undomask", s_editCursorUndo);

		initSuccessful = PositionAtMouseCursor(true) != -1;
		if (initSuccessful)
			SetConfig("undomask", ClearBit(s_editCursorUndo, 3));
	}
	else
	{
		SetConfig("undomask", s_editCursorUndo);
		delete g_moveGridTempoMap;
		g_moveGridTempoMap = NULL;
	}

	g_movedGridOnce = false;
	return initSuccessful;
}

static bool MoveGridDoUndo ()
{
	return g_movedGridOnce;
}

static HCURSOR MoveGridCursor (int window)
{
	static HCURSOR cursor = NULL;
	if (!cursor)
		cursor = LoadCursor(NULL, IDC_SIZEWE);

	if (window == BR_ContinuousAction::ARRANGE || window == BR_ContinuousAction::RULER)
		return cursor;
	else
		return NULL;
}

void MoveGridToMouseInit ()
{
	ContinuousActionRegister(new BR_ContinuousAction(NamedCommandLookup("_BR_MOVE_GRID_TO_MOUSE"),       &MoveGridInit, &MoveGridDoUndo, &MoveGridCursor));
	ContinuousActionRegister(new BR_ContinuousAction(NamedCommandLookup("_BR_MOVE_M_GRID_TO_MOUSE"),     &MoveGridInit, &MoveGridDoUndo, &MoveGridCursor));
	ContinuousActionRegister(new BR_ContinuousAction(NamedCommandLookup("_BR_MOVE_CLOSEST_TEMPO_MOUSE"), &MoveGridInit, &MoveGridDoUndo, &MoveGridCursor));
}

/******************************************************************************
* Commands                                                                    *
******************************************************************************/
static bool MoveTempo (BR_Envelope& tempoMap, int id, double timeDiff)
{
	// Get tempo points
	double t1, t2, t3;
	double b1, b2, b3, Nb1, Nb2;
	int s1, s2;

	tempoMap.GetPoint(id-1, &t1, &b1, &s1, NULL);
	tempoMap.GetPoint(id,   &t2, &b2, &s2, NULL);
	bool P3 = tempoMap.GetPoint(id+1, &t3, &b3, NULL, NULL);
	double Nt2 = t2+timeDiff;

	///// CALCULATE BPM VALUES /////
	////////////////////////////////

	// Current point
	if (P3)
	{
		if (s2 == SQUARE)
			Nb2 = b2*(t3-t2) / (t3-Nt2);
		else
			Nb2 = (b2+b3)*(t3-t2) / (t3-Nt2) - b3;
	}
	else
	{
		Nb2 = b2;
		t3 = Nt2 + 1; // t3 is faked so it can pass legality check
	}

	// Previous point
	if (s1 == SQUARE)
		Nb1 = b1*(t2-t1) / (Nt2-t1);
	else
		Nb1 = (b1+b2)*(t2-t1) / (Nt2-t1) - Nb2;

	// Check if values are legal
	if (Nb2 < MIN_BPM || Nb2 > MAX_BPM || Nb1 < MIN_BPM || Nb1 > MAX_BPM)
		return false;
	if ((Nt2-t1) < MIN_TEMPO_DIST || (t3 - Nt2) < MIN_TEMPO_DIST)
		return false;

	///// CHECK POINTS BEFORE PREVIOUS POINT /////
	/////////////////////////////////////////////

	// Go through points backwards and get new values for linear points
	vector<double> prevBpm;
	bool possible = true;
	int direction = 1;
	for (int i = id-2; i >= 0; --i)
	{
		double b; int s;
		if (!tempoMap.GetPoint(i, NULL, &b, &s, NULL) || s == SQUARE)
			break;
		else
		{
			double newBpm = b - direction*(Nb1 - b1);
			if (newBpm <= MAX_BPM && newBpm >= MIN_BPM)
				prevBpm.push_back(newBpm);
			else
			{
				possible = false;
				break;
			}
		}
		direction *= -1;
	}
	if (!possible)
		return false;

	///// SET NEW BPM VALUES /////
	/////////////////////////////

	// Points before previous (if needed)
	for (size_t i = 0; i < prevBpm.size(); ++i)
		tempoMap.SetPoint(id-2-i, NULL, &prevBpm[i], NULL, NULL);

	// Previous point
	tempoMap.SetPoint(id-1, NULL, &Nb1, NULL, NULL);

	// Selected point
	tempoMap.SetPoint(id, &Nt2, &Nb2, NULL, NULL);

	return true;
}

void MoveGridToMouse (COMMAND_T* ct)
{
	static int    lockedId = -1;
	static double lastPosition = 0;

	// Action called for the first time: reset variables and cache tempo map for future calls
	if (!g_moveGridTempoMap)
	{
		lockedId = -1;
		lastPosition = 0;

		// Make sure tempo map already has at least one point created (for some reason it won't work if creating it directly in chunk)
		if ((int)ct->user != 0) // do it only if not moving tempo marker
			InitTempoMap();

		g_moveGridTempoMap = new (nothrow) BR_Envelope(GetTempoEnv());
		if (!g_moveGridTempoMap || !g_moveGridTempoMap->Count())
		{
			ContinuousActionStopAll();
			return;
		}
	}

	// Find closest grid/tempo marker
	double tDiff = 0;
	double mousePosition = PositionAtMouseCursor(true);
	if (mousePosition == -1)
	{
		ContinuousActionStopAll ();
	}
	else
	{
		// Move action was already called so use data from the previous move
		if (g_movedGridOnce)
		{
			tDiff = mousePosition - lastPosition;
		}

		// Find or create tempo marker to move
		else
		{
			double grid;
			int targetId;

			// Find closest grid tempo marker
			if ((int)ct->user == 1 || (int)ct->user == 2)
			{
				grid = ((int)ct->user == 1) ? (GetClosestGrid(mousePosition)) : (GetClosestMeasureGrid(mousePosition));
				targetId = g_moveGridTempoMap->Find(grid, MIN_TEMPO_DIST);
			}
			// Find closest tempo marker
			else
			{
				targetId = g_moveGridTempoMap->FindClosest(mousePosition);
				if (targetId ==0) ++targetId;
				if (!g_moveGridTempoMap->ValidateId(targetId))
					return;
				g_moveGridTempoMap->GetPoint(targetId, &grid, NULL, NULL, NULL);
			}

			// No tempo marker on grid, create it (skip if moving closest tempo marker)
			if (!g_moveGridTempoMap->ValidateId(targetId))
			{
				int prevId = g_moveGridTempoMap->FindPrevious(grid);
				int shape; g_moveGridTempoMap->GetPoint(prevId, NULL, NULL, &shape, NULL);
				g_moveGridTempoMap->CreatePoint(prevId+1, grid, g_moveGridTempoMap->ValueAtPosition(grid), shape, 0, false);
				targetId = prevId+1;
			}

			// Can't move first tempo marker so ignore this move action and wait for valid mouse position
			if (targetId != 0)
			{
				lockedId = targetId;
				tDiff = mousePosition - grid;
			}
		}
	}

	// Move grid and commit changes
	if (tDiff != 0 && lockedId >= 0)
	{
		// Warn user if tempo marker couldn't get processed
		if (!g_moveGridTempoMap || !MoveTempo(*g_moveGridTempoMap, lockedId, tDiff))
		{
			static bool s_warnUser = true;
			if (s_warnUser)
			{
				ContinuousActionStopAll ();
				int userAnswer = ShowMessageBox(__LOCALIZE("Moving grid failed because some tempo markers would end up with illegal BPM or position. Would you like to be warned if it happens again?", "sws_mbox"), __LOCALIZE("SWS - Warning", "sws_mbox"), 4);
				if (userAnswer == 7)
					s_warnUser = false;
			}
		}
		else
		{
			lastPosition = mousePosition;
			g_moveGridTempoMap->Commit();
			g_movedGridOnce = true;
		}
	}
}

void MoveGridToEditPlayCursor (COMMAND_T* ct)
{
	// Find cursor immediately (in case of playback we want the most accurate position)
	double cursor = ((int)ct->user == 1 || (int)ct->user == 3) ? (GetPlayPositionEx(NULL)) : (GetCursorPositionEx(NULL));

	// Make sure tempo map already has at least one point created (for some reason it won't work if creating it directly in chunk)
	InitTempoMap();
	BR_Envelope tempoMap(GetTempoEnv());
	if (!tempoMap.Count())
		return;

	// Set preferences to prevent play cursor from jumping
	int seekmodes = 0;
	if ((int)ct->user == 1 || (int)ct->user == 3)
	{
		GetConfig("seekmodes", seekmodes);
		SetConfig("seekmodes", ClearBit(seekmodes, 5));
	}

	// Find closest grid
	double grid = 0;
	if      ((int)ct->user == 0 || (int)ct->user == 1) grid = GetClosestGrid(cursor);
	else if ((int)ct->user == 2 || (int)ct->user == 3) grid = GetClosestMeasureGrid(cursor);
	else if ((int)ct->user == 4)                       grid = GetClosestLeftSideGrid(cursor);
	else                                               grid = GetClosestRightSideGrid(cursor);
	int targetId = tempoMap.Find(grid, MIN_TEMPO_DIST);

	// No tempo marker on grid, create it
	if (!tempoMap.ValidateId(targetId))
	{
		int prevId  = tempoMap.FindPrevious(grid);
		double value = tempoMap.ValueAtPosition(grid);
		int shape;
		tempoMap.GetPoint(prevId, NULL, NULL, &shape, NULL);
		tempoMap.CreatePoint(prevId+1, grid, value, shape, 0, false);
		targetId = prevId+1;
	}
	double tDiff = cursor - grid;

	// Commit changes and warn user if needed
	if (tDiff != 0)
	{
		if (MoveTempo(tempoMap, targetId, tDiff))
		{
			PreventUIRefresh(1); // prevent jumpy cursor
			if (tempoMap.Commit())
			{
				// Restore edit cursor only if moving to it
				if ((int)ct->user != 1 && (int)ct->user != 3)
					SetEditCurPos2(NULL, cursor, false, false);
				Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);
			}
			PreventUIRefresh(-1);
		}
		else
		{
			static bool s_warnUser = true;
			if (s_warnUser)
			{
				int userAnswer = ShowMessageBox(__LOCALIZE("Moving grid failed because some tempo markers would end up with illegal BPM or position. Would you like to be warned if it happens again?", "sws_mbox"), __LOCALIZE("SWS - Warning", "sws_mbox"), 4);
				if (userAnswer == 7)
					s_warnUser = false;
			}
		}
	}

	// Restore preferences
	if ((int)ct->user == 1 || (int)ct->user == 3)
		SetConfig("seekmodes", seekmodes);
}

void MoveTempo (COMMAND_T* ct)
{
	BR_Envelope tempoMap(GetTempoEnv());
	if (!tempoMap.Count())
		return;
	double cursor = GetCursorPositionEx(NULL);
	double tDiff = 0;
	int targetId = -1;

	// Find tempo marker closest to the edit cursor
	if ((int)ct->user == 3)
	{
		targetId = tempoMap.FindClosest(cursor);
		if (targetId == 0) ++targetId;
		if (!tempoMap.ValidateId(targetId))
			return;

		double cTime; tempoMap.GetPoint(targetId, &cTime, NULL, NULL, NULL);
		tDiff = cursor - cTime;
	}

	// Just get time difference for selected points
	else
	{
		if ((int)ct->user == 2 || (int)ct->user == -2)
			tDiff = 1 / GetHZoomLevel() * (double)ct->user / 2;
		else
			tDiff = (double)ct->user/10000;
	}

	if (tDiff == 0)
		return;

	// Loop through selected points
	int skipped = 0;
	int count = (targetId != -1) ? (1) : (tempoMap.CountSelected());
	for (int i = 0; i < count; ++i)
	{
		if (int id = ((int)ct->user == 3) ? (targetId) : (tempoMap.GetSelected(i))) // skip first point
			skipped += (MoveTempo(tempoMap, id, tDiff)) ? (0) : (1);
	}

	// Commit changes
	PreventUIRefresh(1); // prevent jumpy cursor
	if (tempoMap.Commit())
	{
		if ((int)ct->user == 3)
			SetEditCurPos2(NULL, cursor, false, false); // always keep cursor position when moving to closest tempo marker
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);
	}
	PreventUIRefresh(-1);

	// Warn user if some points weren't processed
	static bool s_warnUser = true;
	if (s_warnUser && skipped != 0)
	{
		char buffer[512];
		_snprintfSafe(buffer, sizeof(buffer), __LOCALIZE_VERFMT("%d of the selected points didn't get processed because some points would end up with illegal BPM or position. Would you like to be warned if it happens again?", "sws_mbox"), skipped);
		int userAnswer = ShowMessageBox(buffer, __LOCALIZE("SWS - Warning", "sws_mbox"), 4);
		if (userAnswer == 7)
			s_warnUser = false;
	}
}

void EditTempo (COMMAND_T* ct)
{
	// Get tempo map
	BR_Envelope tempoMap(GetTempoEnv());
	if (!tempoMap.CountSelected())
		return;

	// Get values and type of operation to be performed
	bool percentage = false;
	double diff;
	if (GetFirstDigit((int)ct->user) == 1)
		diff = (double)ct->user / 1000;
	else
	{
		diff = (double)ct->user / 200000;
		percentage = true;
	}

	// Loop through selected points and perform BPM calculations
	int skipped = 0;
	for (int i = 0; i < tempoMap.CountSelected(); ++i)
	{
		int id = tempoMap.GetSelected(i);

		// Hold new values for selected and surrounding points
		double Nt1, Nt3, Nb1, Nb3;
		vector<double> selPos;
		vector<double> selBpm;

		///// CURRENT POINT /////
		/////////////////////////

		// Get all consequentially selected points into a vector with their new values. In case
		// there is consequential selection, middle musical position of a transition between first
		// and last selected point will preserve it's time position (where possible)
		int offset = 0;
		if (tempoMap.GetSelection(id+1))
		{
			// Store new values for selected points into vectors
			double musicalMiddle = 0;
			vector<double> musicalLength;
			while (true)
			{
				// Once unselected point is encountered, break
				if (!tempoMap.GetSelection(id+offset))
				{
					--offset;    // since this point is not taken into account, correct offset

					i += offset; // in case of consequential selection, points are treated as
					break;       // one big transition, so skip them all
				}

				// Get point currently in the loop and surrounding points
				double t0, t1, t2, b0, b1, b2;
				int s0, s1;
				bool P0 = tempoMap.GetPoint(id+offset-1, &t0, &b0, &s0, NULL);
				tempoMap.GetPoint(id+offset,   &t1, &b1, &s1, NULL);
				tempoMap.GetPoint(id+offset+1, &t2, &b2, NULL, NULL);

				// Get musical length for every transition but the last
				double mLen = 0;
				if (tempoMap.GetSelection(id+offset+1))
				{
					if (s1 == SQUARE)
						mLen = b1*(t2-t1) / 240;
					else
						mLen = (b1+b2)*(t2-t1) / 480;
				}
				musicalMiddle += mLen;

				// Get new BPM
				double bpm = b1 + (percentage ? (b1*diff) : (diff));
				if (bpm < MIN_BPM)
					bpm = MIN_BPM;
				else if (bpm > MAX_BPM)
					bpm = MAX_BPM;

				// Get new position (special case for the first point)
				double position;
				if (!offset)
				{
					if (P0 && s0 == LINEAR)
						position = (t1*(b0+b1) + t0*(bpm-b1)) / (b0 + bpm);
					else
						position = t1;
				}
				else
				{
					if (s0 == LINEAR)
						position = (480*musicalLength.back() + selPos.back()*(bpm + selBpm.back())) / (bpm + selBpm.back());
					else
						position = (240*musicalLength.back() + selPos.back()*selBpm.back()) / selBpm.back();
				}

				// Store new values
				selPos.push_back(position);
				selBpm.push_back(bpm);
				musicalLength.push_back(mLen);
				++offset;
			}

			// Find time position of musical middle and move all point so time position
			// of musical middle is preserved (only if previous point exists)
			musicalMiddle /= 2;
			if (tempoMap.GetPoint(id-1, NULL, NULL, NULL, NULL))
			{
				double temp = 0;
				for (int i = 0; i < (int)selPos.size()-1; ++i)
				{
					temp += musicalLength[i];
					if (temp >= musicalMiddle)
					{
						// Get length between the points that contain musical middle
						double len = musicalMiddle - (temp-musicalLength[i]);

						// Find original time position of musical middle
						double t0, t1, b0, b1; int s0;
						tempoMap.GetPoint(id+i,   &t0, &b0, &s0, NULL);
						tempoMap.GetPoint(id+i+1, &t1, &b1, NULL, NULL);
						double prevPos = t0 + PositionAtMeasure (b0, (s0 == 1 ? b0 : b1), t1-t0, len);

						// Find new time position of musical middle
						double newPos = t0 + PositionAtMeasure (selBpm[i], (s0 == 1 ? selBpm[i] : selBpm[i+1]), selPos[i+1] - selPos[i], len);

						// Reset time positions of selected points
						double diff = newPos - prevPos;
						for (size_t i = 0; i < selPos.size(); ++i)
							selPos[i] -= diff;
						break;
					}
				}
			}
		}
		else
		{
			// Get selected point
			double t, b;
			tempoMap.GetPoint(id, &t, &b, NULL, NULL);

			// Get new BPM
			b += (percentage) ? (b*diff) : (diff);
			if (b < MIN_BPM)
				b = MIN_BPM;
			else if (b > MAX_BPM)
				b = MAX_BPM;

			// Store it
			selPos.push_back(t);
			selBpm.push_back(b);
		}

		///// PREVIOUS POINT /////
		//////////////////////////

		// Get points before selected points
		double t0, t1, b0, b1;
		int s0, s1;
		bool P0 = tempoMap.GetPoint(id-2, &t0, &b0, &s0, NULL);
		bool P1 = tempoMap.GetPoint(id-1, &t1, &b1, &s1, NULL);

		if (P1)
		{
			// Get first selected point (old and new)
			double t2, b2;
			tempoMap.GetPoint(id, &t2, &b2, NULL, NULL);
			double Nb2 = selBpm.front();
			double Nt2 = selPos.front();

			// If point behind previous doesn't exist, fake it as square
			if (!P0)
				s0 = SQUARE;

			// Calculate new value and position for previous point
			if (!P0 || s0 == SQUARE)
			{
				if (s1 == SQUARE)
				{
					Nt1 = t1;
					Nb1 = b1*(t2-t1) / (Nt2-Nt1);
				}
				else
				{
					Nt1 = t1;
					Nb1 = (b1+b2)*(t2-t1) / (Nt2-Nt1) - Nb2;
				}
			}
			else
			{
				if (s1 == SQUARE)
				{
					double f1 = (b0+b1) *(t1-t0);
					double f2 = b1*(t2-t1);
					double a = b0;
					double b = (a*(t0+Nt2) + f1+f2) / 2;
					double c = a*(t0*Nt2) + f1*Nt2 + f2*t0;
					Nt1 = c / (b + sqrt(pow(b,2) - a*c));
					Nb1 = f2 / (Nt2 - Nt1);
				}
				else
				{
					double f1 = (b0+b1)*(t1-t0);
					double f2 = (b1+b2)*(t2-t1);
					double a = b0 - Nb2;
					double b = (a*(t0+Nt2) + f1+f2) / 2;
					double c = a*(t0*Nt2) + f1*Nt2 + f2*t0;
					Nt1 = c / (b + sqrt(pow(b,2) - a*c));
					Nb1 = f2 / (Nt2 - Nt1) - Nb2;
				}
			}

			// If point behind previous doesn't exist, fake it's position so it can pass legality check
			if (!P0)
				t0 = Nt1 - 1;

			// Check new value is legal
			if (Nb1 > MAX_BPM || Nb1 < MIN_BPM)
				SKIP(skipped, offset+1);
			if ((Nt1-t0) < MIN_TEMPO_DIST || (Nt2 - Nt1) < MIN_TEMPO_DIST)
				SKIP(skipped, offset+1 );
		}

		///// NEXT POINT /////
		//////////////////////

		// Get points after selected points
		double t3, t4, b3, b4;
		int s3;
		bool P3 = tempoMap.GetPoint(id+offset+1, &t3, &b3, &s3, NULL);
		bool P4 = tempoMap.GetPoint(id+offset+2, &t4, &b4, NULL, NULL);

		if (P3)
		{
			// Get last selected point (old and new)
			double t2, b2; int s2;
			tempoMap.GetPoint(id+offset, &t2, &b2, &s2, NULL);
			double Nb2 = selBpm.back();
			double Nt2 = selPos.back();

			// Calculate new value and position for next point
			if (s2 == SQUARE)
			{
				if (P4)
				{
					if (s3 == SQUARE)
					{
						Nt3 = (b2*(t3-t2) + Nb2*Nt2) / Nb2;
						Nb3 = b3*(t4-t3) / (t4-Nt3);
					}
					else
					{
						Nt3 = (b2*(t3-t2) + Nb2*Nt2) / Nb2;
						Nb3 = (b3+b4)*(t4-t3) / (t4-Nt3) - b4;
					}
				}
				else
				{
					Nt3 = (b2*(t3-t2) + Nb2*Nt2) / Nb2;
					Nb3 = b3;
				}
			}
			else
			{
				if (P4)
				{
					if (s3 == SQUARE)
					{
						double f1 = (b2+b3)*(t3-t2);
						double f2 = b3*(t4-t3);
						double a = Nb2;
						double b = (a*(Nt2+t4) + f1+f2) / 2;
						double c = a*(Nt2*t4) + f1*t4 + f2*Nt2;
						Nt3 = c / (b + sqrt(pow(b,2) - a*c));
						Nb3 = f2 / (t4-Nt3);
					}
					else
					{
						double f1 = (b2+b3)*(t3-t2);
						double f2 = (b3+b4)*(t4-t3);
						double a = Nb2 - b4;
						double b = (a*(Nt2+t4) + f1+f2) / 2;
						double c = a*(Nt2*t4) + f1*t4 + f2*Nt2;
						Nt3 = c / (b + sqrt(pow(b,2) - a*c));
						Nb3 = f2 / (t4-Nt3) - b4;
					}
				}
				else
				{
					Nt3 = t3;
					Nb3 = (b2+b3)*(t3-t2) / (Nt3-Nt2) - Nb2;
				}
			}

			// If point after the next doesn't exist fake it's position so it can pass legality check
			if (!P4)
				t4 = Nt3 + 1;

			// Check new value is legal
			if (Nb3 > MAX_BPM || Nb3 < MIN_BPM)
				SKIP(skipped, offset+1);
			if ((Nt3-Nt2) < MIN_TEMPO_DIST || (t4 - Nt3) < MIN_TEMPO_DIST)
				SKIP(skipped, offset+1);
		}

		///// SET BPM /////
		///////////////////

		// Previous point
		if (P1)
			tempoMap.SetPoint(id-1, &Nt1, &Nb1, NULL, NULL);

		// Current point(s)
		for (int i = 0; i < (int)selPos.size(); ++i)
			tempoMap.SetPoint(id+i, &selPos[i], &selBpm[i], NULL, NULL);

		// Next point
		if (P3)
			tempoMap.SetPoint(id+offset+1, &Nt3, &Nb3, NULL, NULL);
	}

	// Commit changes
	if (tempoMap.Commit())
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);

	// Warn user if some points weren't processed
	static bool s_warnUser = true;
	if (s_warnUser && skipped != 0 && tempoMap.CountSelected() > 1)
	{
		char buffer[512];
		_snprintfSafe(buffer, sizeof(buffer), __LOCALIZE_VERFMT("%d of the selected points didn't get processed because some points would end up with illegal BPM or position. Would you like to be warned if it happens again?", "sws_mbox"), skipped);
		int userAnswer = ShowMessageBox(buffer, __LOCALIZE("SWS - Warning", "sws_mbox"), 4);
		if (userAnswer == 7)
			s_warnUser = false;
	}
}

void EditTempoGradual (COMMAND_T* ct)
{
	// Get tempo map
	BR_Envelope tempoMap(GetTempoEnv());
	if (!tempoMap.CountSelected())
		return;

	// Get values and type of operation to be performed
	bool percentage = false;
	double diff;
	if (GetFirstDigit((int)ct->user) == 1)
		diff = (double)ct->user / 1000;
	else
	{
		diff = (double)ct->user / 200000;
		percentage = true;
	}

	// Loop through selected points and perform BPM calculations
	int skipped = 0;
	for (int i = 0; i < tempoMap.CountSelected(); ++i)
	{
		int id = tempoMap.GetSelected(i);

		// Hold new values for selected and next point
		double Nb1, Nt1;
		vector<double> selPos;
		vector<double> selBpm;

		///// CURRENT POINT /////
		/////////////////////////

		// Store new values for selected points into vectors
		int offset = 0;
		while (true)
		{
			// Get point currently in the loop and points surrounding it
			double t0, t1, t2, b0, b1, b2;
			int s0, s1;
			bool P0 = tempoMap.GetPoint(id+offset-1, &t0, &b0, &s0, NULL);
			tempoMap.GetPoint(id+offset,   &t1, &b1, &s1, NULL);
			tempoMap.GetPoint(id+offset+1, &t2, &b2, NULL, NULL);

			// If square or not selected, break
			if (s1 == SQUARE || !tempoMap.GetSelection(id+offset))
			{
				// If breaking on the first selected point, don't adjust i (so for loop doesn't go backwards).
				i += (offset == 0) ? (0) : (offset-1);
				--offset; // since this point is not taken into account, correct offset
				break;
			}

			// Get new BPM
			double bpm = b1 + ((percentage) ? (b1*diff) : (diff));
			if (bpm < MIN_BPM)
				bpm = MIN_BPM;
			else if (bpm > MAX_BPM)
				bpm = MAX_BPM;

			// Get new position (special case for the first point)
			double position;
			if (offset == 0)
			{
				if (P0 && s0 == LINEAR)
				{
					position = (t1*(b0+b1) + t0*(bpm-b1)) / (b0 + bpm); // first point moves but the one before it
					if (position - t0 < MIN_TEMPO_DIST)                 // doesn't so check if their distance is legal
						break;
				}
				else
					position = t1;
			}
			else
				position = ((b0+b1)*(t1-t0) + selPos.back() * (selBpm.back() + bpm)) / (selBpm.back() + bpm);

			// Store new values
			selPos.push_back(position);
			selBpm.push_back(bpm);
			++offset;
		}

		// Check for illegal position/no linear points encountered (in that case offset is -1 so skipped won't change)
		if (!selPos.size())
			SKIP(skipped, offset+1);

		///// NEXT POINT /////
		//////////////////////

		// Get points after the last selected point
		double t1, t2;
		double b1, b2;
		int s2;
		bool P1 = tempoMap.GetPoint(id+offset+1, &t1, &b1, &s2, NULL);
		bool P2 = tempoMap.GetPoint(id+offset+2, &t2, &b2, NULL, NULL);

		// Calculate new value and position for the next point
		if (P1)
		{
			// Get last selected tempo point (old and new)
			double Nb0 = selBpm.back();
			double Nt0 = selPos.back();
			double t0, b0;
			tempoMap.GetPoint(id+offset, &t0, &b0, NULL, NULL);

			if (P2)
			{
				if (s2 == SQUARE)
				{
					double f1 = (b0+b1)*(t1-t0);
					double f2 = b1*(t2-t1);
					double a = Nb0;
					double b = (a*(Nt0+t2) + f1+f2) / 2;
					double c = a*(Nt0*t2) + f1*t2 + f2*Nt0;
					Nt1 = c / (b + sqrt(pow(b,2) - a*c));
					Nb1 = f2 / (t2-Nt1);
				}
				else
				{
					double f1 = (b0+b1)*(t1-t0);
					double f2 = (b1+b2)*(t2-t1);
					double a = Nb0 - b2;
					double b = (a*(Nt0+t2) + f1+f2) / 2;
					double c = a*(Nt0*t2) + f1*t2 + f2*Nt0;
					Nt1 = c / (b + sqrt(pow(b,2) - a*c));
					Nb1 = f2 / (t2-Nt1) - b2;
				}
			}
			else
			{
				Nt1 = t1;
				Nb1 = (b0+b1)*(t1-t0) / (t1-Nt0) - Nb0;
			}

			// If points after selected point don't exist, fake them
			if (!P1)
				Nt1 = Nt0 + 1;
			if (!P2)
				t2 = Nt1 + 1;

			// Check new value is legal
			if (Nb1 > MAX_BPM || Nb1 < MIN_BPM)
				SKIP(skipped, offset+1);
			if ((Nt1-Nt0) < MIN_TEMPO_DIST || (t2 - Nt1) < MIN_TEMPO_DIST)
				SKIP(skipped, offset+1);
		}

		///// SET NEW BPM /////
		///////////////////////

		// Current point(s)
		for (int i = 0; i < (int)selPos.size(); ++i)
			tempoMap.SetPoint(id+i, &selPos[i], &selBpm[i], NULL, NULL);

		// Next point
		if (P1)
			tempoMap.SetPoint(id+offset+1, &Nt1, &Nb1, NULL, NULL);
	}

	// Commit changes
	if (tempoMap.Commit())
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);

	// Warn user if some points weren't processed
	static bool s_warnUser = true;
	if (s_warnUser && skipped != 0 && tempoMap.CountSelected() > 1 )
	{
		char buffer[512];
		_snprintfSafe(buffer, sizeof(buffer), __LOCALIZE_VERFMT("%d of the selected points didn't get processed because some points would end up with illegal BPM or position. Would you like to be warned if it happens again?", "sws_mbox"), skipped);
		int userAnswer = ShowMessageBox(buffer, __LOCALIZE("SWS - Warning", "sws_mbox"), 4);
		if (userAnswer == 7)
			s_warnUser = false;
	}
}

void DeleteTempo (COMMAND_T* ct)
{
	// Get tempo map
	BR_Envelope tempoMap(GetTempoEnv());
	if (!tempoMap.CountSelected())
		return;
	int offset = 0;

	// Loop through selected points and perform BPM calculations
	int skipped = 0;
	for (int i = 0; i < tempoMap.CountSelected(); ++i)
	{
		int id = tempoMap.GetSelected(i) + offset;
		if (id == 0)
			continue;

		// Get tempo points
		double t1, t2, t3, t4;
		double b1, b2, b3, b4;
		int s0, s1, s2, s3;

		tempoMap.GetPoint(id, &t2, &b2, &s2, NULL);
		bool P0 = tempoMap.GetPoint(id-2, NULL, NULL, &s0, NULL);
		bool P1 = tempoMap.GetPoint(id-1, &t1, &b1, &s1, NULL);
		bool P3 = tempoMap.GetPoint(id+1, &t3, &b3, &s3, NULL);
		bool P4 = tempoMap.GetPoint(id+2, &t4, &b4, NULL, NULL);

		// Hold new values
		double Nt1, Nb1;
		double Nt3, Nb3;

		// If previous point doesn't exist, fake it
		if (!P0)
			s0 = SQUARE;

		// Get P2-P3 length
		double m2;
		if (s2 == SQUARE)
			m2 = b2*(t3-t2) / 240;
		else
			m2 = (b2+b3)*(t3-t2) / 480;


		///// CALCULATE BPM VALUES /////
		////////////////////////////////
		if (P3)
		{
			if (s0 == SQUARE)
			{
				if (s1 == SQUARE)
				{
					Nt1 = t1;
					Nb1 = (240*m2 + b1*(t2-t1)) / (t3-t1);
				}
				else
				{
					Nt1 = t1;
					Nb1 = (480*m2 + (b1+b2)*(t2-t1)) / (t3-t1) - b3;
				}

				// Check new value is legal
				if (Nb1 > MAX_BPM || Nb1 < MIN_BPM)
					SKIP(skipped, 1);

				// Next point stays the same
				P3 = false;
			}
			else
			{
				// If P4 exists...
				if (P4)
				{
					if (s1 == SQUARE)
					{
						if (s3 == SQUARE)
						{
							Nt3 = t2 + 240*m2 / b1;
							Nb3 = b3*(t4-t3) / (t4-Nt3);
						}
						else
						{
							Nt3 = t2 + 240*m2 / b1;
							Nb3 = (b3+b4)*(t4-t3) / (t4-Nt3) - b4;
						}
					}
					else
					{
						if (s3 == SQUARE)
						{
							double f1 = (b1+b2)*(t2-t1) + 480*m2;
							double f2 = b3*(t4-t3);
							double a = b1;
							double b = (a*(t1+t4) + f1+f2) / 2;
							double c = a*(t1*t4) + f1*t4 + f2*t1;
							Nt3 = c / (b + sqrt(pow(b,2) - a*c));
							Nb3 = f2 / (t4 - Nt3);
						}
						else
						{
							double f1 = (b1+b2)*(t2-t1) + 480*m2;
							double f2 = (b3+b4)*(t4-t3);
							double a = b1-b4;
							double b = (a*(t1+t4) + f1+f2) / 2;
							double c = a*(t1*t4) + f1*t4 + f2*t1;
							Nt3 = c / (b + sqrt(pow(b,2) - a*c));
							Nb3 = f2 / (t4 - Nt3) - b4;
						}
					}

					// Check new position is legal
					if ((Nt3 - t1) < MIN_TEMPO_DIST || (t4 - Nt3) < MIN_TEMPO_DIST)
						SKIP(skipped, 1);
				}

				// If P4 does not exist
				else
				{

					if (s1 == SQUARE)
					{
						Nt3 = t2 + 240*m2 / b1;
						Nb3 = b3;
					}
					else
					{
						Nt3 = t3;
						Nb3 = (480*m2 + (b1+b2)*(t2-t1)) / (t3-t1) - b1;
					}

					// Check new position is legal
					if ((Nt3 - t1) < MIN_TEMPO_DIST)
						SKIP(skipped, 1);
				}

				// Check new value is legal
				if (Nb3 > MAX_BPM || Nb3 < MIN_BPM)
					SKIP(skipped, 1);

				// Previous point stays the same
				P1 = false;
			}
		}
		else
		{
			// No surrounding points get edited
			P1 = false;
			P3 = false;
		}

		///// SET NEW BPM /////
		///////////////////////

		// Previous point
		if (P1)
			tempoMap.SetPoint(id-1, &Nt1, &Nb1, NULL, NULL);

		// Next point
		if (P3)
			tempoMap.SetPoint(id+1, &Nt3, &Nb3, NULL, NULL);

		// Delete point
		tempoMap.DeletePoint(id);
		--offset;
	}

	// Commit changes
	if (tempoMap.Commit())
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);

	// Warn user if some points weren't processed
	static bool s_warnUser = true;
	if (s_warnUser && skipped != 0)
	{
		char buffer[512];
		_snprintfSafe(buffer, sizeof(buffer), __LOCALIZE_VERFMT("%d of the selected points didn't get processed because some points would end up with illegal BPM or position. Would you like to be warned if it happens again?", "sws_mbox"), skipped);
		int userAnswer = ShowMessageBox(buffer, __LOCALIZE("SWS - Warning", "sws_mbox"), 4);
		if (userAnswer == 7)
			s_warnUser = false;
	}
}

void DeleteTempoPreserveItems (COMMAND_T* ct)
{
	BR_Envelope tempoMap(GetTempoEnv());
	if (!tempoMap.CountSelected())
		return;

	// Get items' position info and set their timebase to time
	vector<BR_MidiItemTimePos> items;
	double firstMarker;
	tempoMap.GetPoint(tempoMap.GetSelected(0), &firstMarker, NULL, NULL, NULL);

	int itemCount = ((int)ct->user) ? CountSelectedMediaItems(NULL) : CountMediaItems(NULL);
	items.reserve(itemCount);
	for (int i = 0; i < itemCount; ++i)
	{
		MediaItem* item = ((int)ct->user) ? GetSelectedMediaItem(NULL, i) : GetMediaItem(NULL, i);
		double itemEnd = GetMediaItemInfo_Value(item, "D_POSITION") + GetMediaItemInfo_Value(item, "D_LENGTH");
		if (itemEnd >= firstMarker)
		{
			items.push_back(BR_MidiItemTimePos(item));
			SetMediaItemInfo_Value(item, "C_BEATATTACHMODE", 0);
		}
	}

	// Readjust unselected tempo markers
	double offset = 0;
	for (int i = 0; i < tempoMap.CountConseq(); ++i)
	{
		int startId, endId;
		tempoMap.GetConseq (i, &startId, &endId);

		if (endId == tempoMap.Count()-1) continue;         // no points after selection, nothing to adjust
		if (startId == 0 && (++startId > endId)) continue; // skip first point

		// Get musical length of selection
		double musicalLen = 0;
		for (int i = startId - 1; i <= endId; ++i )
		{
			double t0, t1, b0, b1; int s0;
			tempoMap.GetPoint(i,   &t0, &b0, &s0, NULL);
			tempoMap.GetPoint(i+1, &t1, &b1, NULL, NULL);
			if (i == startId-1) t0 -= offset; // readjust position to original (earlier iterations moved it)

			if (s0 == SQUARE)
				musicalLen += (t1-t0) * b0 / 240;
			else
				musicalLen += (t1-t0) * (b0+b1) / 480;
		}

		// Readjust points after selection
		double t0, t1, b0, b1; int s0;
		tempoMap.GetPoint(startId - 1, &t0, &b0, &s0, NULL);
		tempoMap.GetPoint(endId   + 1, &t1, &b1, NULL, NULL);

		if (s0 == SQUARE)
			offset = (t0 + (240*musicalLen) / b0) - t1;
		else
			offset = (t0 + (480*musicalLen) / (b0 + b1)) - t1;

		while (!tempoMap.GetSelection(++endId) && endId < tempoMap.Count())
		{
			double t;
			tempoMap.GetPoint(endId, &t, NULL, NULL, NULL);
			t += offset;
			tempoMap.SetPoint (endId, &t, NULL, NULL, NULL);
		}
	}

	// Delete selected tempo markers
	int idOffset = 0;
	for (int i = 0; i < tempoMap.CountSelected(); ++i)
	{
		int id = tempoMap.GetSelected(i) - idOffset;
		if (id != 0) // skip first point
		{
			tempoMap.DeletePoint(id);
			++idOffset;
		}
	}

	// Commit tempo map and restore position info
	PreventUIRefresh(1);
	if (tempoMap.Commit(false))
	{
		for (size_t i = 0; i < items.size(); ++i)
			items[i].Restore();
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);
	}
	PreventUIRefresh(-1);
}

void TempoAtGrid (COMMAND_T* ct)
{
	// Get tempo map and grid
	BR_Envelope tempoMap(GetTempoEnv());
	if (!tempoMap.CountSelected())
		return;
	double grid; GetConfig("projgriddiv", grid);

	// Loop through selected points
	Undo_BeginBlock2(NULL);
	int count = tempoMap.Count()-1;
	for (int i = 0; i < tempoMap.CountSelected(); ++i)
	{
		// Get tempo points
		int id = tempoMap.GetSelected(i);
		double t0, t1, b0, b1;
		int s0;

		tempoMap.GetPoint(id, &t0, &b0, &s0, NULL);
		tempoMap.GetPoint(id+1, &t1, &b1, NULL, NULL);

		// If last tempo point is selected, fake second point as if it's at the end of project (markers and regions included)
		if (id == count)
		{
			b1 = b0;
			t1 = EndOfProject(true, true);
		}

		// "fake" bpm for second point (needed for TempoAtPosition)
		if (s0 == SQUARE)
			b1 = b0;

		// Grid diving starts again from the start of the measure in which tempo marker is so we need to calculate
		// the offset between where grid line really is and where it would be if we just divided QN with grid spacing.
		double beat; int measure; GetTempoTimeSigMarker(NULL, id, NULL, &measure, &beat, NULL, NULL, NULL, NULL);
		double offset =  grid - fmod(TimeMap_timeToQN(TimeMap2_beatsToTime(0, 0, &measure)), grid);

		// Find first grid line and then loop through the rest creating tempo points
		double pGridLn = t0, gridLn = TimeMap_timeToQN(t0);
		gridLn = TimeMap_QNToTime(gridLn-offset - fmod(gridLn,grid)); // it can be before tempo point but next while should correct that
		while (true)
		{
			// Search for the next grid line
			while (gridLn < pGridLn + MAX_GRID_DIV) // MAX_GRID_DIV acts as safety net to prevent accidental multiple points around grid lines/tempo points
				gridLn = TimeMap_QNToTime(TimeMap_timeToQN(gridLn) + grid);

			// Create points until the next point
			if (gridLn < t1 - MAX_GRID_DIV)
				tempoMap.CreatePoint(tempoMap.Count(), gridLn, TempoAtPosition(b0, b1, t0, t1, gridLn), s0, 0, false);
			else
				break;
			pGridLn = gridLn;
		}
	}
	tempoMap.Commit();
	Undo_EndBlock2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL);
}

void TempoShapeLinear (COMMAND_T* ct)
{
	// Get tempo map
	BR_Envelope tempoMap (GetTempoEnv());
	if (!tempoMap.CountSelected())
		return;

	// Get splitting options
	double splitRatio;
	bool split = GetTempoShapeOptions(&splitRatio);

	// Loop through selected points and perform BPM calculations
	int skipped = 0;
	int count = tempoMap.Count()-1;
	for (int i = 0; i < tempoMap.CountSelected(); ++i)
	{
		int id = tempoMap.GetSelected(i);

		// Skip selected point if already linear
		double t0, b0; int s0;
		tempoMap.GetPoint(id, &t0, &b0, &s0, NULL);
		if (s0 == LINEAR)
			continue;
		else
			s0 = LINEAR;

		// Get next point
		double t1, b1; bool P1;
		if (id < count) // since we're creating points at the end of tempo map, this will let us know if we are dealing with the last point
			P1 = tempoMap.GetPoint(id+1, &t1, &b1, NULL, NULL);
		else
			P1 = false;

		// Create middle point(s) if needed
		if (P1 && b0 != b1)
		{
			// Get middle point's position and BPM
			double position, bpm, measure = b0*(t1-t0) / 240;
			FindMiddlePoint(&position, &bpm, measure, t0, t1, b0, b1);

			// Don't split middle point
			if (!split)
			{
				// Check if value and position is legal, if not, skip
				if (bpm>=MIN_BPM && bpm<=MAX_BPM && (position-t0)>=MIN_TEMPO_DIST && (t1-position)>=MIN_TEMPO_DIST)
					tempoMap.CreatePoint(tempoMap.Count(), position, bpm, LINEAR, 0, false);
				else
					SKIP(skipped, 1);
			}

			// Split middle point
			else
			{
				double position1, position2, bpm1, bpm2;
				SplitMiddlePoint (&position1, &position2, &bpm1, &bpm2, splitRatio, measure, t0, position, t1, b0, bpm, b1);

				// Check if value and position is legal, if not, skip
				if (bpm1>=MIN_BPM && bpm1<=MAX_BPM && bpm2>=MIN_BPM && bpm2<=MAX_BPM && (position1-t0)>=MIN_TEMPO_DIST && (position2-position1)>=MIN_TEMPO_DIST && (t1-position2)>=MIN_TEMPO_DIST)
				{
					tempoMap.CreatePoint(tempoMap.Count(), position1, bpm1, LINEAR, 0, false);
					tempoMap.CreatePoint(tempoMap.Count(), position2, bpm2, LINEAR, 0, false);
				}
				else
					SKIP(skipped, 1);
			}
		}

		// Change shape of the selected point
		tempoMap.SetPoint(id, NULL, NULL, &s0, NULL);
	}

	// Commit changes
	if (tempoMap.Commit())
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);

	// Warn user if some points weren't processed
	static bool s_warnUser = true;
	if (s_warnUser && skipped != 0)
	{
		char buffer[512];
		_snprintfSafe(buffer, sizeof(buffer), __LOCALIZE_VERFMT("%d of the selected points didn't get processed because some points would end up with illegal BPM or position. Would you like to be warned if it happens again?", "sws_mbox"), skipped);
		int userAnswer = ShowMessageBox(buffer, __LOCALIZE("SWS - Warning", "sws_mbox"), 4);
		if (userAnswer == 7)
			s_warnUser = false;
	}
}

void TempoShapeSquare (COMMAND_T* ct)
{
	// Get tempo map
	BR_Envelope tempoMap (GetTempoEnv());
	if (!tempoMap.CountSelected())
		return;

	// Get splitting options
	double splitRatio;
	bool split = GetTempoShapeOptions(&splitRatio);

	// Loop through selected points and perform BPM calculations
	int skipped = 0;
	int count = tempoMap.Count()-1;
	for (int i = 0; i < tempoMap.CountSelected(); ++i)
	{
		int id = tempoMap.GetSelected(i);

		// Skip selected point if already square
		double t1, b1; int s1;
		tempoMap.GetPoint(id, &t1, &b1, &s1, NULL);
		if (s1 == SQUARE)
			continue;
		else
			s1 = SQUARE;

		// Get next point
		double b2; bool P2;
		if (id < count) // since we're creating points at the end of tempo map, check if dealing with the last point
			P2 = tempoMap.GetPoint(id+1, NULL, &b2, NULL, NULL);
		else
			P2 = false;

		// Get previous point
		double t0, b0; int s0;
		bool P0 = tempoMap.GetPoint(id-1, &t0, &b0, &s0, NULL);

		// Get new bpm of selected point
		double Nb1;
		if (P2 && b1 != b2)
			Nb1 = (b1+b2) / 2;
		else
			Nb1 = b1;

		// Check if new bpm is legal, if not, skip
		if (Nb1 < MIN_BPM || Nb1 > MAX_BPM)
			SKIP(skipped, 1);

		///// SET NEW SHAPE /////
		/////////////////////////

		// Create middle point(s) is needed
		if (P0 && s0 == LINEAR && P2 && Nb1 != b2)
		{
			// Get middle point's position and BPM
			double position, bpm = 120, measure = (b0+b1)*(t1-t0) / 480;
			FindMiddlePoint(&position, &bpm, measure, t0, t1, b0, Nb1);

			// Don't split middle point
			if (!split)
			{
				if (bpm<= MAX_BPM && bpm>=MIN_BPM && (position-t0)>=MIN_TEMPO_DIST && (t1-position)>=MIN_TEMPO_DIST)
					tempoMap.CreatePoint(tempoMap.Count(), position, bpm, LINEAR, 0, false);
				else
					SKIP(skipped, 1);
			}

			// Split middle point
			else
			{
				double position1, position2, bpm1, bpm2;
				SplitMiddlePoint (&position1, &position2, &bpm1, &bpm2, splitRatio, measure, t0, position, t1, b0, bpm, Nb1);

				if (bpm1>=MIN_BPM && bpm1<=MAX_BPM && bpm2>=MIN_BPM && bpm2<=MAX_BPM && (position1-t0)>=MIN_TEMPO_DIST && (position2-position1)>=MIN_TEMPO_DIST && (t1-position2)>=MIN_TEMPO_DIST)
				{
					tempoMap.CreatePoint(tempoMap.Count(), position1, bpm1, LINEAR, 0, false);
					tempoMap.CreatePoint(tempoMap.Count(), position2, bpm2, LINEAR, 0, false);
				}
				else
					SKIP(skipped, 1);
			}
		}

		// Change shape of the selected point
		tempoMap.SetPoint(id, NULL, &Nb1, &s1, NULL);
	}

	// Commit changes
	if (tempoMap.Commit())
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);

	// Warn user if some points weren't processed
	static bool s_warnUser = true;
	if (s_warnUser && skipped != 0)
	{
		char buffer[512];
		_snprintfSafe(buffer, sizeof(buffer), __LOCALIZE_VERFMT("%d of the selected points didn't get processed because some points would end up with illegal BPM or position. Would you like to be warned if it happens again?", "sws_mbox"), skipped);
		int userAnswer = ShowMessageBox(buffer, __LOCALIZE("SWS - Warning", "sws_mbox"), 4);
		if (userAnswer == 7)
			s_warnUser = false;
	}
}

/******************************************************************************
* Dialogs                                                                     *
******************************************************************************/
void ConvertMarkersToTempoDialog (COMMAND_T* = NULL)
{
	static HWND hwnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_BR_MARKERS_TO_TEMPO), g_hwndParent, ConvertMarkersToTempoProc);

	if (g_convertMarkersToTempoDialog)
	{
		KillTimer(hwnd, 1);
		ShowWindow(hwnd, SW_HIDE);
		g_convertMarkersToTempoDialog = false;
	}

	else
	{
		// Detect timebase
		bool cancel = false;
		int timebase; GetConfig("itemtimelock", timebase);
		if (timebase)
		{
			int answer = MessageBox(g_hwndParent, __LOCALIZE("Project timebase is not set to time. Do you want to set it now?","sws_DLG_166"), __LOCALIZE("SWS - Warning","sws_mbox"), MB_YESNOCANCEL);
			if (answer == 6)
				SetConfig("itemtimelock", 0);
			if (answer == 2)
				cancel = true;
		}

		if (!cancel)
		{
			SetTimer(hwnd, 1, 100, NULL);
			ShowWindow(hwnd, SW_SHOW);
			SetFocus(hwnd);
			g_convertMarkersToTempoDialog = true;
		}
	}
	RefreshToolbar(SWSGetCommandID(ConvertMarkersToTempoDialog));
}

void SelectAdjustTempoDialog (COMMAND_T* = NULL)
{
	static HWND hwnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_BR_SELECT_ADJUST_TEMPO), g_hwndParent, SelectAdjustTempoProc);

	if (g_selectAdjustTempoDialog)
	{
		KillTimer(hwnd, 1);
		UnselectNthDialog(false, hwnd); // hide child dialog
		ShowWindow(hwnd, SW_HIDE);
		g_selectAdjustTempoDialog = false;
	}
	else
	{
		SetTimer(hwnd, 1, 500, NULL);
		ShowWindow(hwnd, SW_SHOW);
		SetFocus(hwnd);
		g_selectAdjustTempoDialog = true;
	}
	RefreshToolbar(SWSGetCommandID(SelectAdjustTempoDialog));
}

void RandomizeTempoDialog (COMMAND_T* = NULL)
{
	DialogBox(g_hInst, MAKEINTRESOURCE(IDD_BR_RANDOMIZE_TEMPO), g_hwndParent, RandomizeTempoProc);
}

void TempoShapeOptionsDialog (COMMAND_T* = NULL)
{
	static HWND hwnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_BR_TEMPO_SHAPE_OPTIONS), g_hwndParent, TempoShapeOptionsProc);

	if (g_tempoShapeDialog)
	{
		ShowWindow(hwnd, SW_HIDE);
		g_tempoShapeDialog = false;
	}
	else
	{
		ShowWindow(hwnd, SW_SHOW);
		SetFocus(hwnd);
		g_tempoShapeDialog = true;
	}
	RefreshToolbar(SWSGetCommandID(TempoShapeOptionsDialog));
}

/******************************************************************************
* Toggle states                                                               *
******************************************************************************/
int IsConvertMarkersToTempoVisible (COMMAND_T* = NULL)
{
	return g_convertMarkersToTempoDialog;
}

int IsSelectAdjustTempoVisible (COMMAND_T* = NULL)
{
	return g_selectAdjustTempoDialog;
}

int IsTempoShapeOptionsVisible (COMMAND_T* = NULL)
{
	return g_tempoShapeDialog;
}