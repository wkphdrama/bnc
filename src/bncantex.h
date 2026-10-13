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

#ifndef BNCANTEX_H
#define BNCANTEX_H

#include <QtCore>
#include <QString>
#include <string>
#include <newmat.h>
#include "bncconst.h"
#include "bnctime.h"

class bncAntex {
 public:
  bncAntex(const char* fileName);
  bncAntex();
  ~bncAntex();
  t_irc   readFile(const QString& fileName);
  void    print() const;
  QString pcoSinexString(const std::string& antName, t_frequency::type frqType);
  QString snxCodeSinexString(const std::string& antName);
  double  satCorr(const QString& prn, t_frequency::type frqType,
                  double eleSat, double azSat, bool& found) const;
  double  rcvCorr(const std::string& antName, t_frequency::type frqType,
                  double eleSat, double azSat, bool& found) const;

  // Attitude model selection for satCoMcorrection().
  enum e_attMode {
    ATT_COMPUTED = 0,  // full model: GLONASS yaw-fixed + GPS noon/midnight turn
    ATT_NOMINAL  = 1,  // simple nominal Sun-pointing only (no maneuver model)
    ATT_EXTERNAL = 2,  // use caller-supplied externalYaw angle [rad]
  };

  t_irc   satCoMcorrection(const QString& prn, double Mjd,
                           const ColumnVector& xSat, const ColumnVector& vSat,
                           ColumnVector& dx,
                           e_attMode mode = ATT_COMPUTED,
                           double externalYaw = 0.0);

  // Drain and return diagnostic logs accumulated by the yaw models.
  // Empty most of the time; only non-empty right after a transition.
  QString takeGlonassYawLog() {
    QString s = _glonassYawLog;
    _glonassYawLog.clear();
    return s;
  }
  QString takeGpsYawLog() {
    QString s = _gpsYawLog;
    _gpsYawLog.clear();
    return s;
  }
  QString takeOnYawLog() {
    QString s = _onYawLog;
    _onYawLog.clear();
    return s;
  }

 private:
  // Per-satellite GLONASS yaw state: freezes the yaw angle while the
  // satellite cannot follow the nominal Sun-pointing law (Dilssner 2011).
  class t_glonassYaw {
   public:
    t_glonassYaw() {
      yaw         = 0.0;
      lastCallMjd = 0.0;
      valid       = false;
      fixed       = false;
    }
    double yaw;
    double lastCallMjd; // Mjd of the most recent call (any branch) for this PRN
    bool   valid;
    bool   fixed;   // true while the yaw-fixed override is currently active
  };

  // Per-satellite GPS yaw state: rate-limits the yaw during noon/midnight
  // turns when the required yaw rate exceeds the block's mechanical maximum
  // (Kouba 2009/2015, Bar-Sever 1996).
  class t_gpsYaw {
   public:
    t_gpsYaw() {
      yaw         = 0.0;
      lastCallMjd = 0.0;
      valid       = false;
      inTurn      = false;
    }
    double yaw;
    double lastCallMjd;
    bool   valid;
    bool   inTurn;  // true while in a constrained noon/midnight turn
  };

  // Per-satellite Galileo/BDS yaw state: tracks rate-limited rotation between
  // yaw-steering (psiNom) and orbit-normal mode (psi=0) when |beta| crosses
  // the constellation-specific threshold (Kouba 2017, Dai et al. 2015).
  class t_onYaw {
   public:
    t_onYaw() {
      yaw         = 0.0;
      lastCallMjd = 0.0;
      valid       = false;
      inON        = false;
    }
    double yaw;
    double lastCallMjd;
    bool   valid;
    bool   inON;  // true while satellite is in (or transitioning to) orbit-normal mode
  };

  QString _glonassYawLog;
  QString _gpsYawLog;
  QString _onYawLog;

  double glonassYawAngle(const QString& prn, double Mjd, const ColumnVector& xSat,
                          const ColumnVector& vSat, const ColumnVector& xSun);

  double gpsYawAngle(const QString& prn, const QString& blockType, double Mjd,
                     const ColumnVector& xSat, const ColumnVector& vSat,
                     const ColumnVector& xSun);

  double onModeYawAngle(const QString& prn, double betaThr, double psiDotMax,
                        double Mjd, const ColumnVector& xSat,
                        const ColumnVector& vSat, const ColumnVector& xSun);

  double galileoYawAngle(const QString& prn, const QString& blockType, double Mjd,
                         const ColumnVector& xSat, const ColumnVector& vSat,
                         const ColumnVector& xSun);

  double bdsYawAngle(const QString& prn, const QString& blockType, double Mjd,
                     const ColumnVector& xSat, const ColumnVector& vSat,
                     const ColumnVector& xSun);

  QMap<QString, t_glonassYaw> _glonassYaw;
  QMap<QString, t_gpsYaw>     _gpsYaw;
  QMap<QString, t_onYaw>      _onYaw;

  class t_frqMap {
   public:
    t_frqMap() {
      for (unsigned ii = 0; ii < 3; ii++) {
        neu[ii] = 0.0;
      }
    }
    double       neu[3];
    ColumnVector pattern;
  };

  class t_antMap {
   public:
    t_antMap() {
      zen1    = 0.0;
      zen2    = 0.0;
      dZen    = 0.0;
      snxCode = "";
    }
    ~t_antMap() {
      QMapIterator<t_frequency::type, t_frqMap*> it(frqMap);
      while (it.hasNext()) {
        it.next();
        delete it.value();
      }
    }
    QString                            antName;
    QString                            blockType;  // e.g. "IIF", "IIR-M", "IIA", "IIIA" (GPS only)
    double                             zen1;
    double                             zen2;
    double                             dZen;
    QString                            snxCode;
    QMap<t_frequency::type, t_frqMap*> frqMap;
    bncTime                            validFrom;
    bncTime                            validTo;
  };

  QMap<QString, t_antMap*> _maps;
};

#endif
