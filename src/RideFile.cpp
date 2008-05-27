/* 
 * Copyright (c) 2007 Sean C. Rhea (srhea@srhea.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "RideFile.h"
#include <QtXml/QtXml>
#include <assert.h>

static void
markInterval(QDomDocument &xroot, QDomNode &xride, QDomNode &xintervals,
             double &startSecs, double prevSecs,
             int &thisInterval, RideFilePoint *sample)
{
    if (xintervals.isNull()) {
        xintervals = xroot.createElement("intervals");
        xride.appendChild(xintervals);
    }
    QDomElement xinterval = xroot.createElement("interval").toElement();
    xintervals.appendChild(xinterval);
    xinterval.setAttribute("name", thisInterval);
    xinterval.setAttribute("begin_secs",
                           QString("%1").arg(startSecs, 0, 'f', 2));
    xinterval.setAttribute("end_secs", 
                           QString("%1").arg(prevSecs, 0, 'f', 2));
    startSecs = sample->secs;
    thisInterval = sample->interval;
}

bool
RideFile::writeAsXml(QFile &file, QString &err)
{
    (void) err;
    QDomDocument xroot("GoldenCheetah 1.0");
    QDomNode xride = xroot.createElement("ride");
    xroot.appendChild(xride);
    QDomElement xstart = xroot.createElement("start").toElement();
    xride.appendChild(xstart);
    xstart.setAttribute("date", startTime_.toString("yyyy/MM/dd hh:mm:ss"));
    QDomElement xdevtype = xroot.createElement("device_type").toElement();
    xride.appendChild(xdevtype);
    xdevtype.setAttribute("name", deviceType_);
    QDomElement xrecint = xroot.createElement("sampling_period").toElement();
    xride.appendChild(xrecint);
    xrecint.setAttribute("secs", QString("%1").arg(recIntSecs_, 0, 'f', 3));
    QDomNode xintervals;
    bool hasNm = false;
    QVector<double> intervalStart, intervalStop;
    double startSecs = 0.0, prevSecs = 0.0;
    int thisInterval = 0;
    QListIterator<RideFilePoint*> i(dataPoints_);
    RideFilePoint *sample = NULL;
    while (i.hasNext()) {
        sample = i.next();
        if (sample->nm > 0.0)
            hasNm = true;
        assert(sample->secs >= 0.0);
        if (sample->interval != thisInterval) {
            markInterval(xroot, xride, xintervals, startSecs,
                         prevSecs, thisInterval, sample);
        }
        prevSecs = sample->secs;
    }
    if (sample) {
        markInterval(xroot, xride, xintervals, startSecs,
                     prevSecs, thisInterval, sample);
    }
    QDomNode xsamples = xroot.createElement("samples");
    xride.appendChild(xsamples);
    i.toFront();
    while (i.hasNext()) {
        RideFilePoint *sample = i.next();
        QDomElement xsamp = xroot.createElement("sample").toElement();
        xsamples.appendChild(xsamp);
        xsamp.setAttribute("secs", QString("%1").arg(sample->secs, 0, 'f', 2));
        xsamp.setAttribute("cad", QString("%1").arg(sample->cad, 0, 'f', 0));
        xsamp.setAttribute("hr", QString("%1").arg(sample->hr, 0, 'f', 0));
        xsamp.setAttribute("km", QString("%1").arg(sample->km, 0, 'f', 3));
        xsamp.setAttribute("kph", QString("%1").arg(sample->kph, 0, 'f', 1));
        xsamp.setAttribute("watts", sample->watts);
        if (hasNm) {
            double nm = (sample->watts > 0.0) ? sample->nm : 0.0;
            xsamp.setAttribute("nm", QString("%1").arg(nm, 0,'f', 1));
        }
    }
    file.open(QFile::WriteOnly);
    QTextStream ts(&file);
    xroot.save(ts, 4);
    file.close();
    return true;
}

RideFileFactory *RideFileFactory::instance_;

RideFileFactory &RideFileFactory::instance() 
{ 
    if (!instance_) 
        instance_ = new RideFileFactory();
    return *instance_;
}

int RideFileFactory::registerReader(const QString &suffix, 
                                       RideFileReader *reader) 
{
    assert(!readFuncs_.contains(suffix));
    readFuncs_.insert(suffix, reader);
    return 1;
}

RideFile *RideFileFactory::openRideFile(QFile &file, 
                                           QStringList &errors) const 
{
    QString suffix = file.fileName();
    int dot = suffix.lastIndexOf(".");
    assert(dot >= 0);
    suffix.remove(0, dot + 1);
    RideFileReader *reader = readFuncs_.value(suffix);
    assert(reader);
    return reader->openRideFile(file, errors);
}

QStringList RideFileFactory::listRideFiles(const QDir &dir) const 
{
    QStringList filters;
    QMapIterator<QString,RideFileReader*> i(readFuncs_);
    while (i.hasNext()) {
        i.next();
        filters << ("*." + i.key());
    }
    return dir.entryList(filters, QDir::Files, QDir::Name);
}

