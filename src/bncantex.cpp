// Part of BNC, a utility for retrieving decoding and
// converting GNSS data streams from NTRIP broadcasters.
//
// Copyright (C) 2007
// German Federal Agency for Cartography and Geodesy (BKG)
// http://www.bkg.bund.de
// Czech Technical University Prague, Department of Geodesy
// http://www.fsv.cvut.cz
//
// Email: euref-ip@bkg.bund.de
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation, version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

/* -------------------------------------------------------------------------
 * BKG NTRIP Client
 * -------------------------------------------------------------------------
 *
 * Class:      bncAntex
 *
 * Purpose:    Antenna Phase Centers and Variations from ANTEX File
 *
 * Author:     L. Mervart
 *
 * Created:    26-Jan-2011
 *
 * Changes:
 *
 * -----------------------------------------------------------------------*/

#include <iostream>
#include <cmath>
#include <newmatio.h>

#include "bncantex.h"
#include "pppModel.h"

using namespace std;

// Constructor
////////////////////////////////////////////////////////////////////////////
bncAntex::bncAntex() {
}

// Constructor
////////////////////////////////////////////////////////////////////////////
bncAntex::bncAntex(const char* fileName) {
  readFile(QString(fileName));//print();
}

// Destructor
////////////////////////////////////////////////////////////////////////////
bncAntex::~bncAntex() {
  QMapIterator<QString, t_antMap*> it(_maps);
  while (it.hasNext()) {
    it.next();
    delete it.value();
  }
  _maps.clear();
}

// Print
////////////////////////////////////////////////////////////////////////////
void bncAntex::print() const {
  QMapIterator<QString, t_antMap*> itAnt(_maps);
  while (itAnt.hasNext()) {
    itAnt.next();
    t_antMap* map = itAnt.value();
    cout << map->antName.toLatin1().data() << endl;
    cout << "    " << map->zen1 << " " << map->zen2 << " " << map->dZen << endl;
    QMapIterator<t_frequency::type, t_frqMap*> itFrq(map->frqMap);
    while (itFrq.hasNext()) {
      itFrq.next();
      const t_frqMap* frqMap = itFrq.value();
      cout << t_frequency::toString(itFrq.key()) << ":\n"
           << frqMap->neu[0] << " "
           << frqMap->neu[1] << " "
           << frqMap->neu[2] << endl;
      cout << frqMap->pattern.t();
    }
    cout << endl;
  }
}

// Print
////////////////////////////////////////////////////////////////////////////
QString bncAntex::pcoSinexString(const std::string& antName, t_frequency::type frqType) {

  if (antName.find("NULLANTENNA") != string::npos) {
    return QString(" ------ ------ ------");
  }

  QString antNameQ = antName.c_str();
  if (_maps.find(antNameQ) == _maps.end()) {
    return QString(" ------ ------ ------");
  }

  t_antMap* map = _maps[antNameQ];
  if (map->frqMap.find(frqType) == map->frqMap.end()) {
    return QString(" ------ ------ ------");
  }

  t_frqMap* frqMap = map->frqMap[frqType];

  QString u = QString().asprintf("%+6.4f" ,frqMap->neu[2]); if (u.mid(1,1) == "0") {u.remove(1,1);}
  QString n = QString().asprintf("%+6.4f" ,frqMap->neu[0]); if (n.mid(1,1) == "0") {n.remove(1,1);}
  QString e = QString().asprintf("%+6.4f" ,frqMap->neu[1]); if (e.mid(1,1) == "0") {e.remove(1,1);}

  return QString(" %1 %2 %3").arg(u).arg(n).arg(e);
}

// Print
////////////////////////////////////////////////////////////////////////////
QString bncAntex::snxCodeSinexString(const std::string& antName) {

  if (antName.find("NULLANTENNA") != string::npos) {
    return QString(" ----------");
  }

  QString antNameQ = antName.c_str();
  if (_maps.find(antNameQ) == _maps.end()) {
    return QString(" ----------");
  }
  else {
    return QString(" %1").arg(_maps[antNameQ]->snxCode, 10, QLatin1Char(' '));
  }
}

// Read ANTEX File
////////////////////////////////////////////////////////////////////////////
t_irc bncAntex::readFile(const QString& fileName) {

  QFile inFile(fileName);
  inFile.open(QIODevice::ReadOnly | QIODevice::Text);

  QTextStream in(&inFile);

  t_antMap* newAntMap = 0;
  t_frqMap* newFrqMap = 0;

  while ( !in.atEnd() ) {
    QString line = in.readLine();

    // Start of Antenna
    // ----------------
    if      (line.indexOf("START OF ANTENNA") == 60) {
      if (newAntMap) {
        delete newAntMap;
        return failure;
      }
      else {
        delete newAntMap;
        newAntMap = new t_antMap();
      }
    }

    // End of Antenna
    // --------------
    else if (line.indexOf("END OF ANTENNA") == 60) {
      if (newAntMap) {
        if (_maps.contains(newAntMap->antName)) {
          delete _maps[newAntMap->antName];
        }
        _maps[newAntMap->antName] = newAntMap;
        newAntMap = 0;
      }
      else {
        delete newAntMap;
        return failure;
      }
    }

    // Antenna Reading in Progress
    // ---------------------------
    else if (newAntMap) {
      if      (line.indexOf("TYPE / SERIAL NO") == 60) {
        if (line.indexOf("BLOCK I") == 0 ||
            line.indexOf("GLONASS") == 0 ||
            line.indexOf("QZSS") == 0 ||
            line.indexOf("BEIDOU") == 0 ||
            line.indexOf("GALILEO") == 0 ||
            line.indexOf("NavIC") == 0 ){
          newAntMap->antName = line.mid(20,3);
          if (line.indexOf("BLOCK I") == 0) {
            // Extract GPS block type: "BLOCK IIF   " → "IIF"
            QString bt = line.mid(0, 20).trimmed();
            if (bt.startsWith("BLOCK "))
              newAntMap->blockType = bt.mid(6);
          }
          else if (line.indexOf("GALILEO") == 0) {
            // "GALILEO-1" → "1" (IOV), "GALILEO-2" → "2" (FOC)
            QString bt = line.mid(0, 20).trimmed();
            if (bt.startsWith("GALILEO-"))
              newAntMap->blockType = bt.mid(8);
          }
          else if (line.indexOf("BEIDOU") == 0) {
            // "BEIDOU-2M" → "2M", "BEIDOU-3M-CAST" → "3M-CAST", etc.
            QString bt = line.mid(0, 20).trimmed();
            if (bt.startsWith("BEIDOU-"))
              newAntMap->blockType = bt.mid(7);
          }
        }
        else {
          newAntMap->antName = line.mid(0,20);
        }
      }
      else if (line.indexOf("ZEN1 / ZEN2 / DZEN") == 60) {
        QTextStream inLine(&line, QIODevice::ReadOnly);
        inLine >> newAntMap->zen1 >> newAntMap->zen2 >> newAntMap->dZen;
      }
      else if (line.indexOf("SINEX CODE") == 60) {
        QTextStream inLine(&line, QIODevice::ReadOnly);
        inLine >> newAntMap->snxCode;
      }

      // Start of Frequency
      // ------------------
      else if (line.indexOf("START OF FREQUENCY") == 60) {
        if (newFrqMap) {
          delete newFrqMap;
          delete newAntMap;
          return failure;
        }
        else {
          newFrqMap = new t_frqMap();
        }
      }

      // End of Frequency
      // ----------------
      else if (line.indexOf("END OF FREQUENCY") == 60) {
        if (newFrqMap) {
          t_frequency::type frqType = t_frequency::dummy;
          // GPS
          if      (line.indexOf("G01") == 3) {
            frqType = t_frequency::G1;
          }
          else if (line.indexOf("G02") == 3) {
            frqType = t_frequency::G2;
          }
          else if (line.indexOf("G05") == 3) {
            frqType = t_frequency::G5;
          }
          // GLONASS
          else if (line.indexOf("R01") == 3) {
            frqType = t_frequency::R1;
          }
          else if (line.indexOf("R02") == 3) {
            frqType = t_frequency::R2;
          }
          // Galileo
          else if (line.indexOf("E01") == 3) {
            frqType = t_frequency::E1;
          }
          else if (line.indexOf("E05") == 3) {
            frqType = t_frequency::E5;
          }
          else if (line.indexOf("E06") == 3) {
            frqType = t_frequency::E6;
          }
          else if (line.indexOf("E07") == 3) {
            frqType = t_frequency::E7;
          }
          else if (line.indexOf("E08") == 3) {
            frqType = t_frequency::E8;
          }
          // QZSS
          else if (line.indexOf("J01") == 3) {
            frqType = t_frequency::J1;
          }
          else if (line.indexOf("J02") == 3) {
            frqType = t_frequency::J2;
          }
          else if (line.indexOf("J05") == 3) {
            frqType = t_frequency::J5;
          }
          else if (line.indexOf("J06") == 3) {
            frqType = t_frequency::J6;
          }
          // BDS
          else if (line.indexOf("C01") == 3) {
            frqType = t_frequency::C1;
          }
          else if (line.indexOf("C02") == 3) {
            frqType = t_frequency::C2;
          }
          else if (line.indexOf("C06") == 3) {
            frqType = t_frequency::C6;
          }
          else if (line.indexOf("C07") == 3) {
            frqType = t_frequency::C7;
          }
          if (frqType != t_frequency::dummy) {
            if (newAntMap->frqMap.find(frqType) != newAntMap->frqMap.end()) {
              delete newAntMap->frqMap[frqType];
            }
            newAntMap->frqMap[frqType] = newFrqMap;
          }
          else {
            delete newFrqMap;
          }
          newFrqMap = 0;
        }
        else {
          delete newAntMap;
          return failure;
        }
      }

      // Frequency Reading in Progress
      // -----------------------------
      else if (newFrqMap) {
        if      (line.indexOf("NORTH / EAST / UP") == 60) {
          QTextStream inLine(&line, QIODevice::ReadOnly);
          inLine >> newFrqMap->neu[0] >> newFrqMap->neu[1] >> newFrqMap->neu[2];
          newFrqMap->neu[0] *= 1e-3;
          newFrqMap->neu[1] *= 1e-3;
          newFrqMap->neu[2] *= 1e-3;
        }
        else if (line.indexOf("NOAZI") == 3) {
          QTextStream inLine(&line, QIODevice::ReadOnly);
          int nPat = int((newAntMap->zen2-newAntMap->zen1)/newAntMap->dZen) + 1;
          newFrqMap->pattern.ReSize(nPat);
          QString dummy;
          inLine >> dummy;
          for (int ii = 0; ii < nPat; ii++) {
            inLine >> newFrqMap->pattern[ii];
          }
          newFrqMap->pattern *= 1e-3;
        }
      }
    }
  }
  inFile.close();
  delete newFrqMap;
  delete newAntMap;

  return success;
}

// GLONASS Yaw Angle (Sun-pointing law overridden near noon/midnight when
// the satellite cannot mechanically keep up, following the GLONASS-M
// "yaw-fixed" behaviour described in Dilssner, Springer, Flohrer, Dow
// (2011), "The GLONASS-M satellite yaw-attitude model", Advances in Space
// Research 47(1), 160-171.
//
// Unlike GPS/Galileo/BeiDou, which are commonly approximated by the
// nominal Sun-pointing yaw-steering law of Bar-Sever (1996) at all times,
// GLONASS-M has been found to stop tracking that law and hold a constant
// (frozen) yaw angle whenever the required yaw rate would exceed the
// satellite's slew capability - which happens close to the orbit
// noon/midnight points whenever the Sun's elevation above the orbital
// plane (the "beta" angle) is small. The maximum yaw rate used below
// (0.25 deg/s) and the general approach follow that paper; satellite
// telemetry was not available to validate the exact rate against this
// installation's GLONASS satellites, so it should be checked against
// independently-known attitude/orbit residuals if high accuracy matters.
////////////////////////////////////////////////////////////////////////////
double bncAntex::glonassYawAngle(const QString& prn, double Mjd,
                                  const ColumnVector& xSat,
                                  const ColumnVector& vSat,
                                  const ColumnVector& xSun) {

  const double MAX_YAW_RATE = 0.25 * M_PI / 180.0; // [rad/s], approximate

  // This bounds the gap BETWEEN CONSECUTIVE CALLS for a given satellite
  // (not how long the freeze itself has lasted - a real low-beta passage
  // can legitimately stay frozen far longer than this). If there was no
  // call for longer than this, something else interrupted normal epoch-by-
  // epoch processing (a stream outage, a maneuver-flagged-unhealthy
  // period, ...), so the stored frozen value is more likely stale than a
  // still-valid freeze, and we fall back to the current nominal value
  // instead of trusting it indefinitely.
  const double MAX_CALL_GAP = 1800.0 / 86400.0; // 30 minutes, in days

  // Inertial-consistent velocity (xSat, vSat are Earth-fixed; remove the
  // Earth-rotation contribution so that the orbit normal below is not
  // contaminated by it)
  // -------------------------------------------------------------------
  ColumnVector Omega(3); Omega(1) = 0.0; Omega(2) = 0.0; Omega(3) = t_CST::omega;
  ColumnVector vInert = vSat + crossproduct(Omega, xSat);

  // Orbit normal and instantaneous orbital rate from r x v (exact, valid
  // for any Keplerian orbit, not just circular ones)
  // ---------------------------------------------------------------------
  ColumnVector h = crossproduct(xSat, vInert);
  double       hNorm = sqrt(DotProduct(h, h));
  ColumnVector orbNormal = h / hNorm;
  double       r = sqrt(DotProduct(xSat, xSat));
  double       nRate = hNorm / (r * r); // [rad/s]

  // Beta angle (Sun elevation above the orbital plane)
  // ----------------------------------------------------
  double beta = asin(DotProduct(orbNormal, xSun));

  // Orbit angle mu, measured from the orbit midnight point, increasing in
  // the direction of satellite motion
  // -----------------------------------------------------------------------
  ColumnVector sunProj = xSun - DotProduct(xSun, orbNormal) * orbNormal;
  sunProj /= sqrt(DotProduct(sunProj, sunProj));
  ColumnVector eX = -1.0 * sunProj;                 // midnight direction, mu = 0
  ColumnVector eY = crossproduct(orbNormal, eX);
  ColumnVector rHat = xSat / r;
  double mu = atan2(DotProduct(rHat, eY), DotProduct(rHat, eX));

  // Nominal Sun-pointing yaw angle and its rate (Bar-Sever 1996)
  // ----------------------------------------------------------------
  double tanBeta = tan(beta);
  double sinMu   = sin(mu);
  double psiNom  = atan2(-tanBeta, sinMu);
  double denom   = tanBeta * tanBeta + sinMu * sinMu;
  double psiRate = (denom > 1e-12)
                  ? nRate * fabs(tanBeta * cos(mu)) / denom
                  : 1e9;

  t_glonassYaw& st = _glonassYaw[prn];
  double callGap = st.valid ? (Mjd - st.lastCallMjd) : 0.0;
  double psiEff;
  if (psiRate > MAX_YAW_RATE) {
    if (!st.valid || callGap > MAX_CALL_GAP) {
      if (st.valid) { // i.e. it was a genuine gap, not cold-start
        _glonassYawLog += QString().asprintf(
          "%s glonassYaw STALE-RESET  Mjd=%.6f beta=%6.3f mu=%7.2f rate=%6.3f"
          " old=%7.2f new=%7.2f gap=%.1fmin\n",
          prn.toLatin1().data(), Mjd, beta*180.0/M_PI, mu*180.0/M_PI,
          psiRate*180.0/M_PI, st.yaw*180.0/M_PI, psiNom*180.0/M_PI,
          callGap*1440.0);
      }
      st.yaw = psiNom; // no prior history, or the gap is too long to trust it
    }
    if (!st.fixed) {
      _glonassYawLog += QString().asprintf(
        "%s glonassYaw FIXED-ENTER  Mjd=%.6f beta=%6.3f mu=%7.2f rate=%6.3f yaw=%7.2f\n",
        prn.toLatin1().data(), Mjd, beta*180.0/M_PI, mu*180.0/M_PI,
        psiRate*180.0/M_PI, st.yaw*180.0/M_PI);
      st.fixed = true;
    }
    psiEff = st.yaw;    // hold the frozen yaw angle
  }
  else {
    if (st.fixed) {
      _glonassYawLog += QString().asprintf(
        "%s glonassYaw FIXED-EXIT   Mjd=%.6f beta=%6.3f mu=%7.2f rate=%6.3f"
        " frozen=%7.2f nominal=%7.2f\n",
        prn.toLatin1().data(), Mjd, beta*180.0/M_PI, mu*180.0/M_PI,
        psiRate*180.0/M_PI, st.yaw*180.0/M_PI, psiNom*180.0/M_PI);
      st.fixed = false;
    }
    st.yaw = psiNom;
    psiEff = psiNom;
  }
  st.lastCallMjd = Mjd; // update on every call, fixed or not, to detect gaps
  st.valid       = true;

  return psiEff;
}

// GPS satellite yaw angle during nominal tracking and noon/midnight turns.
//
// GPS satellites perform a "noon turn" (and, for some blocks, a "midnight
// turn") when the Sun's elevation angle above the orbital plane (beta) is
// small: the required nominal yaw rate then exceeds the satellite's
// mechanical maximum, so the satellite yaws at that maximum rate until it
// catches back up to the nominal Sun-pointing orientation (Kouba 2009/2015,
// Bar-Sever 1996).
//
// The max yaw rates below are the best published estimates per block type:
//   IIA:   0.12 °/s (Kouba 2009)
//   IIR:   0.20 °/s (Bar-Sever 1996)
//   IIR-M: 0.20 °/s
//   IIF:   0.11 °/s (Kouba 2015)
//   IIIA:  0.15 °/s (tentative)
//
// Returns the effective yaw angle [rad] in the velocity-referenced frame,
// for use with the same Rodrigues rotation as the GLONASS model. During
// nominal tracking this equals psiNom and the result is identical to the
// simple sz×xSun formula.
////////////////////////////////////////////////////////////////////////////
double bncAntex::gpsYawAngle(const QString& prn, const QString& blockType,
                              double Mjd,
                              const ColumnVector& xSat,
                              const ColumnVector& vSat,
                              const ColumnVector& xSun) {

  // Max yaw rate [rad/s] by GPS block type
  double psiDotMax;
  if      (blockType == "IIA")   psiDotMax = 0.12 * M_PI / 180.0;
  else if (blockType == "IIR")   psiDotMax = 0.20 * M_PI / 180.0;
  else if (blockType == "IIR-M") psiDotMax = 0.20 * M_PI / 180.0;
  else if (blockType == "IIF")   psiDotMax = 0.11 * M_PI / 180.0;
  else if (blockType == "IIIA")  psiDotMax = 0.15 * M_PI / 180.0;
  else return 0.0; // unknown block: caller uses simple Sun-pointing

  const double MAX_CALL_GAP = 1800.0 / 86400.0; // 30 min in days

  // Inertial velocity
  ColumnVector Omega(3); Omega(1) = 0.0; Omega(2) = 0.0; Omega(3) = t_CST::omega;
  ColumnVector vInert = vSat + crossproduct(Omega, xSat);

  // Orbital angular momentum vector → orbital rate
  ColumnVector h     = crossproduct(xSat, vInert);
  double       hNorm = sqrt(DotProduct(h, h));
  ColumnVector orbNormal = h / hNorm;
  double       r    = sqrt(DotProduct(xSat, xSat));
  double       nRate = hNorm / (r * r); // [rad/s]

  // Beta angle
  double beta = asin(DotProduct(orbNormal, xSun));

  // Mu: orbit angle from midnight (same geometry as GLONASS)
  ColumnVector sunProj = xSun - DotProduct(xSun, orbNormal) * orbNormal;
  sunProj /= sqrt(DotProduct(sunProj, sunProj));
  ColumnVector eX = -1.0 * sunProj; // midnight direction
  ColumnVector eY = crossproduct(orbNormal, eX);
  ColumnVector rHat = xSat / r;
  double mu = atan2(DotProduct(rHat, eY), DotProduct(rHat, eX));

  // Nominal yaw and its rate
  double tanBeta = tan(beta);
  double sinMu   = sin(mu);
  double psiNom  = atan2(-tanBeta, sinMu);
  double denom   = tanBeta * tanBeta + sinMu * sinMu;
  // |dPsi/dt| (always non-negative, sign comes from sign of tanBeta*cos(mu))
  double psiDotNomAbs = (denom > 1e-12)
                        ? nRate * fabs(tanBeta * cos(mu)) / denom
                        : 1e9;
  // Sign of the nominal yaw rate: dPsi/dt = nRate * tanBeta * cos(mu) / denom
  double psiDotNomSign = (tanBeta * cos(mu) >= 0.0) ? 1.0 : -1.0;

  t_gpsYaw& st      = _gpsYaw[prn];
  double    callGap = st.valid ? (Mjd - st.lastCallMjd) : 0.0;

  // Stale state: reset if there has been a gap in calls
  if (st.valid && callGap > MAX_CALL_GAP) {
    _gpsYawLog += QString().asprintf(
      "%s gpsYaw STALE-RESET  Mjd=%.6f beta=%6.3f mu=%7.2f"
      " old=%7.2f new=%7.2f gap=%.1fmin\n",
      prn.toLatin1().data(), Mjd, beta*180.0/M_PI, mu*180.0/M_PI,
      st.yaw*180.0/M_PI, psiNom*180.0/M_PI, callGap*1440.0);
    st.valid  = false;
    st.inTurn = false;
  }

  double psiEff;
  if (psiDotNomAbs > psiDotMax) {
    // Constrained: satellite yaws at psiDotMax in the nominal direction
    if (!st.valid || !st.inTurn) {
      // Entering a noon/midnight turn
      psiEff = st.valid ? st.yaw : psiNom;
      _gpsYawLog += QString().asprintf(
        "%s gpsYaw TURN-ENTER   Mjd=%.6f beta=%6.3f mu=%7.2f"
        " psiNom=%7.2f psiEff=%7.2f psiDotNom=%6.3f max=%5.3f\n",
        prn.toLatin1().data(), Mjd, beta*180.0/M_PI, mu*180.0/M_PI,
        psiNom*180.0/M_PI, psiEff*180.0/M_PI,
        psiDotNomAbs*psiDotNomSign*180.0/M_PI, psiDotMax*180.0/M_PI);
      st.inTurn = true;
    } else {
      // Continuing the turn: integrate at constrained rate
      double dt  = callGap * 86400.0; // [s]
      psiEff = st.yaw + psiDotNomSign * psiDotMax * dt;
    }
    // Wrap to [-pi, pi]
    while (psiEff >  M_PI) psiEff -= 2.0 * M_PI;
    while (psiEff < -M_PI) psiEff += 2.0 * M_PI;
  }
  else {
    // Nominal tracking
    psiEff = psiNom;
    if (st.inTurn) {
      _gpsYawLog += QString().asprintf(
        "%s gpsYaw TURN-EXIT    Mjd=%.6f beta=%6.3f mu=%7.2f"
        " frozen=%7.2f nominal=%7.2f\n",
        prn.toLatin1().data(), Mjd, beta*180.0/M_PI, mu*180.0/M_PI,
        st.yaw*180.0/M_PI, psiNom*180.0/M_PI);
      st.inTurn = false;
    }
    st.yaw = psiNom;
  }

  st.yaw         = psiEff;
  st.lastCallMjd = Mjd;
  st.valid       = true;

  return psiEff;
}

//
// Orbit-Normal Mode Yaw Angle — shared model for Galileo and BDS
//
// When |beta| < betaThr the satellite rotates toward yaw = 0 (orbit-normal)
// at the block-specific maximum yaw rate. When |beta| >= betaThr it returns
// to nominal yaw-steering (psiNom). Transitions are rate-limited in both
// directions to match physical satellite behaviour.
//
// References:
//   Galileo IOV/FOC: Kouba (2017), Steigenberger et al. (2018)
//   BDS MEO/IGSO:    Dai et al. (2015), Wang et al. (2018)
////////////////////////////////////////////////////////////////////////////
double bncAntex::onModeYawAngle(const QString& prn,
                                 double betaThr, double psiDotMax, double Mjd,
                                 const ColumnVector& xSat,
                                 const ColumnVector& vSat,
                                 const ColumnVector& xSun) {

  const double MAX_CALL_GAP = 1800.0 / 86400.0;  // 30 min [days]

  // Inertial velocity
  ColumnVector Omega(3); Omega(1) = 0.0; Omega(2) = 0.0; Omega(3) = t_CST::omega;
  ColumnVector vInert = vSat + crossproduct(Omega, xSat);

  // Orbit geometry
  ColumnVector h        = crossproduct(xSat, vInert);
  double       hNorm    = sqrt(DotProduct(h, h));
  ColumnVector orbNormal = h / hNorm;
  double       r        = sqrt(DotProduct(xSat, xSat));

  double beta = asin(DotProduct(orbNormal, xSun));

  ColumnVector sunProj = xSun - DotProduct(xSun, orbNormal) * orbNormal;
  sunProj /= sqrt(DotProduct(sunProj, sunProj));
  ColumnVector eX   = -1.0 * sunProj;
  ColumnVector eY   = crossproduct(orbNormal, eX);
  ColumnVector rHat = xSat / r;
  double mu     = atan2(DotProduct(rHat, eY), DotProduct(rHat, eX));
  double psiNom = atan2(-tan(beta), sin(mu));

  // Target yaw: orbit-normal (0) when below threshold, nominal otherwise
  bool   wantsON   = (fabs(beta) < betaThr);
  double psiTarget = wantsON ? 0.0 : psiNom;

  t_onYaw& st      = _onYaw[prn];
  double   callGap = st.valid ? (Mjd - st.lastCallMjd) : 0.0;

  if (st.valid && callGap > MAX_CALL_GAP) {
    _onYawLog += QString().asprintf(
      "%s onYaw STALE-RESET  Mjd=%.6f beta=%5.2f gap=%.1fmin\n",
      prn.toLatin1().data(), Mjd, beta*180.0/M_PI, callGap*1440.0);
    st.valid = false;
  }

  double psiEff;
  if (!st.valid) {
    // Cold start: initialise at psiNom regardless of mode
    psiEff    = psiNom;
    st.inON   = false;
  }
  else {
    double dt      = callGap * 86400.0;       // [s]
    double dpsi    = psiTarget - st.yaw;
    while (dpsi >  M_PI) dpsi -= 2.0 * M_PI;
    while (dpsi < -M_PI) dpsi += 2.0 * M_PI;
    double maxDpsi = psiDotMax * dt;

    if (fabs(dpsi) <= maxDpsi) {
      // Target reached this step
      psiEff = psiTarget;
      bool wasON = st.inON;
      st.inON    = wantsON;
      if (!wasON && wantsON && fabs(st.yaw) < 0.5*M_PI/180.0) {
        _onYawLog += QString().asprintf(
          "%s onYaw ON-ENTER   Mjd=%.6f beta=%5.2f psiNom=%7.2f\n",
          prn.toLatin1().data(), Mjd, beta*180.0/M_PI, psiNom*180.0/M_PI);
      }
      else if (wasON && !wantsON) {
        _onYawLog += QString().asprintf(
          "%s onYaw ON-EXIT    Mjd=%.6f beta=%5.2f psiNom=%7.2f\n",
          prn.toLatin1().data(), Mjd, beta*180.0/M_PI, psiNom*180.0/M_PI);
        st.inON = false;
      }
    }
    else {
      // Still rotating toward target
      psiEff = st.yaw + (dpsi > 0.0 ? 1.0 : -1.0) * maxDpsi;
    }
  }

  while (psiEff >  M_PI) psiEff -= 2.0 * M_PI;
  while (psiEff < -M_PI) psiEff += 2.0 * M_PI;

  st.yaw         = psiEff;
  st.lastCallMjd = Mjd;
  st.valid       = true;

  return psiEff;
}

//
////////////////////////////////////////////////////////////////////////////
double bncAntex::galileoYawAngle(const QString& prn, const QString& blockType,
                                  double Mjd,
                                  const ColumnVector& xSat,
                                  const ColumnVector& vSat,
                                  const ColumnVector& xSun) {
  // IOV (type "1") and FOC (type "2"): orbit-normal for |beta| < 2 deg.
  // Max yaw rate 0.20 deg/s applies to both generations.
  // Kouba (2017), Steigenberger et al. (2018)
  Q_UNUSED(blockType);
  const double betaThr   = 2.0 * M_PI / 180.0;
  const double psiDotMax = 0.20 * M_PI / 180.0;
  return onModeYawAngle(prn, betaThr, psiDotMax, Mjd, xSat, vSat, xSun);
}

//
////////////////////////////////////////////////////////////////////////////
double bncAntex::bdsYawAngle(const QString& prn, const QString& blockType,
                              double Mjd,
                              const ColumnVector& xSat,
                              const ColumnVector& vSat,
                              const ColumnVector& xSun) {
  // GEO satellites are always in orbit-normal mode (yaw = 0).
  if (blockType == "2G" || blockType == "3G-CAST")
    return 0.0;

  double betaThr, psiDotMax;
  if (blockType == "2M" || blockType == "2I") {
    // BDS-2 MEO / IGSO: orbit-normal for |beta| < 4 deg (Dai et al. 2015)
    betaThr   = 4.0 * M_PI / 180.0;
    psiDotMax = 0.10 * M_PI / 180.0;
  }
  else {
    // BDS-3 (CAST and SECM MEO/IGSO): orbit-normal for |beta| < 3 deg
    betaThr   = 3.0 * M_PI / 180.0;
    psiDotMax = 0.15 * M_PI / 180.0;
  }
  return onModeYawAngle(prn, betaThr, psiDotMax, Mjd, xSat, vSat, xSun);
}

//
// Satellite Antenna Offset
////////////////////////////////////////////////////////////////////////////
t_irc bncAntex::satCoMcorrection(const QString& prn, double Mjd,
                                 const ColumnVector& xSat,
                                 const ColumnVector& vSat, ColumnVector& dx,
                                 e_attMode mode, double externalYaw) {

  t_frequency::type frqType = t_frequency::dummy;

  if      (prn[0] == 'G') {
    frqType = t_frequency::G1;
  }
  else if (prn[0] == 'R') {
    frqType = t_frequency::R1;
  }
  else if (prn[0] == 'E') {
    frqType = t_frequency::E1;
  }
  else if (prn[0] == 'C') {
    frqType = t_frequency::C2;
  }
  else if (prn[0] == 'S') {
    frqType = t_frequency::S1;
  }
  else if (prn[0] == 'J') {
    frqType = t_frequency::J1;
  }
  else if (prn[0] == 'I') {
    frqType = t_frequency::I5;
  }

  QMap<QString, t_antMap*>::const_iterator it = _maps.find(prn.mid(0,3));
  if (it != _maps.end()) {
    t_antMap* map = it.value();
    if (map->frqMap.find(frqType) != map->frqMap.end()) {

      double* neu = map->frqMap[frqType]->neu;

      // Unit Vectors sz, sy, sx
      // -----------------------
      ColumnVector sz = -xSat;
      sz /= sqrt(DotProduct(sz,sz));

      ColumnVector xSun = BNC_PPP::t_astro::Sun(Mjd);
      xSun /= sqrt(DotProduct(xSun,xSun));

      ColumnVector sy, sx;

      // Determine the satellite body frame orientation.
      //
      // ATT_NOMINAL: simple nominal Sun-pointing for all systems.
      // ATT_COMPUTED or ATT_EXTERNAL: use per-system attitude models.
      //   GLONASS  → yaw-fixed model (Dilssner et al. 2011)
      //   GPS      → noon/midnight turn model (Kouba 2009/2015)
      //   others   → simple nominal Sun-pointing
      // ATT_EXTERNAL: caller supplies the yaw angle [rad] directly
      //   (velocity-referenced frame, same convention as GLONASS/GPS models).
      // -----------------------------------------------------------------------
      bool useVelocityFrame = false;
      double psiEff = 0.0;

      if (mode == ATT_COMPUTED && prn[0] == 'R' && vSat.size() == 3) {
        psiEff = glonassYawAngle(prn, Mjd, xSat, vSat, xSun);
        useVelocityFrame = true;
      }
      else if (mode == ATT_COMPUTED && prn[0] == 'G' && vSat.size() == 3
               && !map->blockType.isEmpty()) {
        psiEff = gpsYawAngle(prn, map->blockType, Mjd, xSat, vSat, xSun);
        useVelocityFrame = true;
      }
      else if (mode == ATT_COMPUTED && prn[0] == 'E' && vSat.size() == 3
               && !map->blockType.isEmpty()) {
        psiEff = galileoYawAngle(prn, map->blockType, Mjd, xSat, vSat, xSun);
        useVelocityFrame = true;
      }
      else if (mode == ATT_COMPUTED && prn[0] == 'C' && vSat.size() == 3
               && !map->blockType.isEmpty()) {
        psiEff = bdsYawAngle(prn, map->blockType, Mjd, xSat, vSat, xSun);
        useVelocityFrame = true;
      }
      else if (mode == ATT_EXTERNAL && vSat.size() == 3) {
        psiEff = externalYaw;
        useVelocityFrame = true;
      }

      if (useVelocityFrame) {
        ColumnVector Omega(3); Omega(1) = 0.0; Omega(2) = 0.0; Omega(3) = t_CST::omega;
        ColumnVector vInert = vSat + crossproduct(Omega, xSat);

        ColumnVector sy0 = crossproduct(sz, vInert);
        sy0 /= sqrt(DotProduct(sy0,sy0));
        ColumnVector sx0 = crossproduct(sy0, sz);

        // Rodrigues rotation of (sx0, sy0) around sz by psiEff
        double cosY = cos(psiEff);
        double sinY = sin(psiEff);
        sx = sx0 * cosY + crossproduct(sz, sx0) * sinY;
        sy = sy0 * cosY + crossproduct(sz, sy0) * sinY;
      }
      else {
        // Nominal Sun-pointing: direct formula (ATT_NOMINAL, or computed
        // with no block-type-specific model available)
        sy = crossproduct(sz, xSun);
        sy /= sqrt(DotProduct(sy,sy));
        sx = crossproduct(sy, sz);
      }

      dx[0] = sx[0] * neu[0] + sy[0] * neu[1] + sz[0] * neu[2];
      dx[1] = sx[1] * neu[0] + sy[1] * neu[1] + sz[1] * neu[2];
      dx[2] = sx[2] * neu[0] + sy[2] * neu[1] + sz[2] * neu[2];

      return success;
    }
  }

  return failure;
}

//
////////////////////////////////////////////////////////////////////////////
double bncAntex::satCorr(const QString& prn, t_frequency::type frqType,
                         double elTx, double azTx, bool& found) const {

  if (_maps.find(prn.mid(0,3)) == _maps.end()) {
    found = false;
    return 0.0;
  };

  t_antMap* map = _maps[prn.mid(0,3)];

  if (map->frqMap.find(frqType) == map->frqMap.end()) {
    found = false;
    return 0.0;
  };

  t_frqMap* frqMap = map->frqMap[frqType];

  double var = 0.0;
  if (frqMap->pattern.ncols() > 0) {
    double zenDiff = 999.999;
    double zenTx  = 90.0 - elTx * 180.0 / M_PI;
    unsigned iZen = 0;
    for (double zen = map->zen1; zen <= map->zen2; zen += map->dZen) {
      iZen += 1;
      double newZenDiff = fabs(zen - zenTx);
      if (newZenDiff < zenDiff) {
        zenDiff = newZenDiff;
        var = frqMap->pattern(iZen);
      }
    }
  }

  found = true;
  return var - frqMap->neu[0] * cos(azTx)*cos(elTx)
             - frqMap->neu[1] * sin(azTx)*cos(elTx)
             - frqMap->neu[2] * sin(elTx);

}

//
////////////////////////////////////////////////////////////////////////////
double bncAntex::rcvCorr(const string& antName, t_frequency::type frqType,
                         double eleSat, double azSat, bool& found) const {

  if (antName.find("NULLANTENNA") != string::npos) {
    found = true;
    return 0.0;
  }

  QString antNameQ = antName.c_str();

  if (_maps.find(antNameQ) == _maps.end()) {
    found = false;
    return 0.0;
  }

  t_antMap* map = _maps[antNameQ];
  if (map->frqMap.find(frqType) == map->frqMap.end()) {
    found = false;
    return 0.0;
  }

  t_frqMap* frqMap = map->frqMap[frqType];

  double var = 0.0;
  if (frqMap->pattern.ncols() > 0) {
    double zenDiff = 999.999;
    double zenSat  = 90.0 - eleSat * 180.0 / M_PI;
    unsigned iZen = 0;
    for (double zen = map->zen1; zen <= map->zen2; zen += map->dZen) {
      iZen += 1;
      double newZenDiff = fabs(zen - zenSat);
      if (newZenDiff < zenDiff) {
        zenDiff = newZenDiff;
        var = frqMap->pattern(iZen);
      }
    }
  }

  found = true;
  return var - frqMap->neu[0] * cos(azSat)*cos(eleSat)
             - frqMap->neu[1] * sin(azSat)*cos(eleSat)
             - frqMap->neu[2] * sin(eleSat);

}
