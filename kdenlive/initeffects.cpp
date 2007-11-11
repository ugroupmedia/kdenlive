/***************************************************************************
                          initeffects.cpp  -  description
                             -------------------
    begin                :  Jul 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include <qfile.h>
#include <qregexp.h>
#include <qdir.h>

#include <kdebug.h>
#include <kglobal.h>
#include <kstandarddirs.h>

#include "initeffects.h"
#include "krender.h"

initEffects::initEffects()
{
}

initEffects::~initEffects()
{
}

//static
void initEffects::parseEffectFiles(EffectDescriptionList *effectList)
{
    QStringList::Iterator more;
    QStringList::Iterator it;
    QStringList fileList;
    QString itemName;


    // Build effects. Retrieve the list of MLT's available effects first.

    QString datFile = KdenliveSettings::mltpath() + "/share/mlt/modules/filters.dat";

    QStringList filtersList;

    QFile file( datFile );
    if ( file.open( IO_ReadOnly ) ) {
        QTextStream stream( &file );
        QString line;
        while ( !stream.atEnd() ) {
            line = stream.readLine(); // line of text excluding '\n'
            filtersList<<line.section(QRegExp("\\s+"), 0, 0);
        }
        file.close();
    }

    // Build effects. check producers first.
    datFile = KdenliveSettings::mltpath() + "/share/mlt/modules/producers.dat";
    QStringList producersList;

    file.setName( datFile );
    if ( file.open( IO_ReadOnly ) ) {
        QTextStream stream( &file );
        QString line;
        while ( !stream.atEnd() ) {
            line = stream.readLine(); // line of text excluding '\n'
            producersList<<line.section(QRegExp("\\s+"), 0, 0);
        }
        file.close();
    }


    KGlobal::dirs()->addResourceType("ladspa_plugin", "lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/usr/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/usr/local/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/opt/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/opt/local/lib/ladspa");

    kdDebug()<<"//  INIT EFFECT SEARCH"<<endl;

    QStringList direc = KGlobal::dirs()->findDirs("data", "kdenlive/effects");
    kdDebug()<<"//  FOUND DIRECTORIES: "<<direc<<endl;
    QDir directory;
    for ( more = direc.begin() ; more != direc.end() ; ++more ) {
	directory = QDir(*more);
	fileList = directory.entryList( QDir::Files );
	for ( it = fileList.begin() ; it != fileList.end() ; ++it ){
	    itemName = KURL(*more + *it).path();
	    parseEffectFile(effectList, itemName, filtersList, producersList);
	    kdDebug()<<"//  FOUND EFFECT FILE: "<<itemName<<endl;
	}
    }
}

// static
void initEffects::parseEffectFile(EffectDescriptionList *effectList, QString name, QStringList filtersList, QStringList producersList)
{
    QDomDocument doc;
    QFile file(name);
    doc.setContent(&file, false);

    QDomElement documentElement = doc.documentElement();
    while (!documentElement.isNull() && documentElement.tagName() != "effect") {
	documentElement = documentElement.firstChild().toElement();
    }
    if (documentElement.isNull()) {
	kdDebug()<<"// EFFECT FILET: "<<name<<" IS BROKEN"<<endl;
	return;
    }
    QString tag = documentElement.attribute("tag", QString::null);
    bool ladspaOk = true;
    if (tag == "ladspa") {
	QString library = documentElement.attribute("library", QString::null);
	if (locate("ladspa_plugin", library).isEmpty()) ladspaOk = false;
    }

    // Parse effect file
    if (filtersList.findIndex(tag) != -1 && ladspaOk) {
	kdDebug()<<"++ ADDING EFFECT: "<<tag<<endl;
    	QDomNode n = documentElement.firstChild();
	QString id, effectName, effectTag, paramType;
	int paramCount = 0;
	EFFECTTYPE type;
        QXmlAttributes xmlAttr;

        // Create Effect
        EffectParamDescFactory effectDescParamFactory;
        EffectDesc *effect = NULL;

    	while (!n.isNull()) {
	    QDomElement e = n.toElement();
	    if (!e.isNull()) {
	        if (e.tagName() == "name") {
		    effectName = i18n(e.text());
		}
	        if (e.tagName() == "properties") {
		    id = e.attribute("id", QString::null);
		    effectTag = e.attribute("tag", QString::null);
		    if (e.attribute("type", QString::null) == "audio") type = AUDIOEFFECT;
		    else type = VIDEOEFFECT;
		    effect = new EffectDesc(effectName, id, effectTag, type);
		}
	        if (e.tagName() == "parameter" && effect) {
		    paramCount++;
		    xmlAttr.clear();
		    
		    QDomNamedNodeMap attrs = e.attributes();
		    int i = 0;
		    QString value;
		    while (!attrs.item(i).isNull()) {
			QDomNode n = attrs.item(i);
			value = n.nodeValue();
			if (value.find("MAX_WIDTH") != -1)
			    value.replace("MAX_WIDTH", QString::number(KdenliveSettings::defaultwidth()));
			if (value.find("MID_WIDTH") != -1)
			    value.replace("MID_WIDTH", QString::number(KdenliveSettings::defaultwidth() / 2));
			if (value.find("MAX_HEIGHT") != -1)
			    value.replace("MAX_HEIGHT", QString::number(KdenliveSettings::defaultheight()));
			if (value.find("MID_HEIGHT") != -1)
			    value.replace("MID_HEIGHT", QString::number(KdenliveSettings::defaultheight() / 2));
			xmlAttr.append(n.nodeName(), QString::null, QString::null, value);
			i++;
		    }
		    QDomNode n2 = n.firstChild();
		    QDomElement e2 = n2.toElement();
	    	    if (!e2.isNull() && e2.tagName() == "name") {
			xmlAttr.append(QString("description"), QString::null, QString::null, i18n(e2.text()));
		    }
        	    effect->addParameter(effectDescParamFactory.createParameter(xmlAttr));
		}
	    }
	    n = n.nextSibling();
	}
	if (paramCount == 0) {
	    xmlAttr.append("type", QString::null, QString::null, "fixed");
            effect->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	}
        effectList->append(effect);
    }
}


//static 
void initEffects::initializeEffects(EffectDescriptionList *effectList)
{

    /**

	This lists all effects that will be available in  Kdenlive.
	To add a new effect, do something like this:

	1) create effect entry
	
	EffectDesc *my_effect = new EffectDesc(i18n("display_name"), "mlt_name", "effect_type");
	
	where display_name is the name of the effect that will be displayed in Kdenlive
	mlt_name is the effect name in MLT, which will be used during creation of the playlist
	effect_type is either AUDIOEFFECT or VIDEOEFFECT, depending on whether the effect affects auido or video,  which allows kdenlive to sort effects according to their type.

	2) Add parameters.
	
	Each effect can have several parameters. The parameter types are:
	
	* fixed : There are no parameters to this effect
	* constant : a integer number that will remains constant during all effect duration.
		Constant effects have parameters: name is the MLT param name, description is the param name to be displayed in ui, max, min and default are self describing, factor is the number that will divide your param before passing it to MLT. For example if you need a parameter between 0.0 and 1.0, you give a min of 0, a max of 100 and a factor of 100.
	* double : The value of this parameter will change during the effect. User can create keyframes. NB: currently, there can only be one double parameter in your effect, and it must be inserted as the first parameter of the effect.
	* complex : this is a special case that was created for the MLT obscure effect
	* bool: a boolean value that will be represented ba a checkbox

    **/
/*
    QXmlAttributes xmlAttr;
    EffectParamDescFactory effectDescParamFactory;

    if (filtersList.findIndex("greyscale") != -1) {
        // Grayscale
        EffectDesc *grey = new EffectDesc(i18n("Greyscale"), "greyscale", "greyscale", VIDEOEFFECT);
        xmlAttr.append("type", QString::null, QString::null, "fixed");
        grey->addParameter(effectDescParamFactory.createParameter(xmlAttr));
        effectList->append(grey);
    }

    if (filtersList.findIndex("invert") != -1) {
        // Invert
        EffectDesc *invert = new EffectDesc(i18n("Invert"), "invert", "invert", VIDEOEFFECT);
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "fixed");
        invert->addParameter(effectDescParamFactory.createParameter(xmlAttr));
        effectList->append(invert);
    }


    if (filtersList.findIndex("threshold") != -1) {
        // Threshold
        EffectDesc *threshold = new EffectDesc(i18n("Threshold"), "threshold", "threshold", VIDEOEFFECT);
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "midpoint");
        xmlAttr.append("description", QString::null, QString::null, i18n("Threshold value"));
        xmlAttr.append("max", QString::null, QString::null, "255");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "128");
	threshold->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "bool");
	xmlAttr.append("name", QString::null, QString::null, "use_alpha");
	xmlAttr.append("description", QString::null, QString::null, i18n("Use transparency"));
	xmlAttr.append("default", QString::null, QString::null, "0");
	threshold->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(threshold);
    }

    if (filtersList.findIndex("chroma") != -1) {
        // chroma
        EffectDesc *chroma = new EffectDesc(i18n("Blue Screen"), "chroma", "chroma", VIDEOEFFECT);
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "color");
        xmlAttr.append("name", QString::null, QString::null, "key");
        xmlAttr.append("description", QString::null, QString::null, i18n("Color key"));
        xmlAttr.append("default", QString::null, QString::null, "0x000000");
	chroma->addParameter(effectDescParamFactory.createParameter(xmlAttr));
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "variance");
        xmlAttr.append("description", QString::null, QString::null, i18n("Variance"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("factor", QString::null, QString::null, "100");
        xmlAttr.append("default", QString::null, QString::null, "15");
	chroma->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(chroma);
    }

    if (filtersList.findIndex("chroma_hold") != -1) {
        // chroma hold
        EffectDesc *chroma_hold = new EffectDesc(i18n("Chroma Hold"), "chroma_hold", "chroma_hold", VIDEOEFFECT);
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "color");
        xmlAttr.append("name", QString::null, QString::null, "key");
        xmlAttr.append("description", QString::null, QString::null, i18n("Color key"));
        xmlAttr.append("default", QString::null, QString::null, "0x000000");
	chroma_hold->addParameter(effectDescParamFactory.createParameter(xmlAttr));
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "variance");
        xmlAttr.append("description", QString::null, QString::null, i18n("Variance"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("factor", QString::null, QString::null, "100");
        xmlAttr.append("default", QString::null, QString::null, "15");
	chroma_hold->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(chroma_hold);
    }

    if (filtersList.findIndex("sepia") != -1) {
        // Sepia
        EffectDesc *sepia = new EffectDesc(i18n("Sepia"), "sepia", "sepia", VIDEOEFFECT);
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "u");
        xmlAttr.append("description", QString::null, QString::null, i18n("Chrominance U"));
        xmlAttr.append("max", QString::null, QString::null, "255");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "75");
	sepia->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "v");
	xmlAttr.append("description", QString::null, QString::null, i18n("Chrominance V"));
	xmlAttr.append("max", QString::null, QString::null, "255");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "150");
	sepia->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(sepia);
    }

    if (filtersList.findIndex("affine") != -1) {
        // Rotate
        EffectDesc *rotate = new EffectDesc(i18n("Rotate"), "affine", "affine", VIDEOEFFECT);
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "transition.rotate_x");
        xmlAttr.append("description", QString::null, QString::null, i18n("Rotation x"));
        xmlAttr.append("max", QString::null, QString::null, "200");
        xmlAttr.append("min", QString::null, QString::null, "-200");
        xmlAttr.append("default", QString::null, QString::null, "0");
        xmlAttr.append("factor", QString::null, QString::null, "10");
	rotate->addParameter(effectDescParamFactory.createParameter(xmlAttr));
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "transition.rotate_y");
        xmlAttr.append("description", QString::null, QString::null, i18n("Rotation y"));
        xmlAttr.append("max", QString::null, QString::null, "200");
        xmlAttr.append("min", QString::null, QString::null, "-200");
        xmlAttr.append("default", QString::null, QString::null, "0");
        xmlAttr.append("factor", QString::null, QString::null, "10");
	rotate->addParameter(effectDescParamFactory.createParameter(xmlAttr));
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "transition.rotate_z");
        xmlAttr.append("description", QString::null, QString::null, i18n("Rotation z"));
        xmlAttr.append("max", QString::null, QString::null, "200");
        xmlAttr.append("min", QString::null, QString::null, "-200");
        xmlAttr.append("default", QString::null, QString::null, "0");
        xmlAttr.append("factor", QString::null, QString::null, "10");
	rotate->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(rotate);
    }

    if (filtersList.findIndex("charcoal") != -1) {
	// Charcoal
	EffectDesc *charcoal = new EffectDesc(i18n("Charcoal"), "charcoal", "charcoal", VIDEOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "x_scatter");
	xmlAttr.append("description", QString::null, QString::null, i18n("Horizontal scatter"));
	xmlAttr.append("max", QString::null, QString::null, "10");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "2");
	charcoal->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "y_scatter");
	xmlAttr.append("description", QString::null, QString::null, i18n("Vertical scatter"));
	xmlAttr.append("max", QString::null, QString::null, "10");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "2");
	charcoal->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "scale");
	xmlAttr.append("description", QString::null, QString::null, i18n("Scale"));
	xmlAttr.append("max", QString::null, QString::null, "10");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "1");
	charcoal->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "mix");
	xmlAttr.append("description", QString::null, QString::null, i18n("Mix"));
	xmlAttr.append("max", QString::null, QString::null, "10");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "0");
	charcoal->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "bool");
	xmlAttr.append("name", QString::null, QString::null, "invert");
	xmlAttr.append("description", QString::null, QString::null, i18n("Invert"));
	xmlAttr.append("default", QString::null, QString::null, "0");
	charcoal->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(charcoal);
    }

    if (filtersList.findIndex("brightness") != -1) {
	// Brightness
	EffectDesc *bright = new EffectDesc(i18n("Brightness"), "brightness", "brightness", VIDEOEFFECT);
	xmlAttr.clear();

	xmlAttr.append("type", QString::null, QString::null, "double");
	xmlAttr.append("name", QString::null, QString::null, "intensity");
	xmlAttr.append("description", QString::null, QString::null, i18n("Intensity"));
	xmlAttr.append("max", QString::null, QString::null, "300");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "100");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	bright->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(bright);
    }

    if (filtersList.findIndex("boxblur") != -1) {
        // box blur
        EffectDesc *blur = new EffectDesc(i18n("Box Blur"), "boxblur", "boxblur", VIDEOEFFECT);
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "double");
        xmlAttr.append("name", QString::null, QString::null, "blur");
        xmlAttr.append("description", QString::null, QString::null, i18n("Blur factor"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "5");
        blur->addParameter(effectDescParamFactory.createParameter(xmlAttr));
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "hori");
        xmlAttr.append("description", QString::null, QString::null, i18n("Horizontal multiplicator"));
        xmlAttr.append("max", QString::null, QString::null, "50");
        xmlAttr.append("min", QString::null, QString::null, "1");
        xmlAttr.append("default", QString::null, QString::null, "1");
        blur->addParameter(effectDescParamFactory.createParameter(xmlAttr));
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "vert");
        xmlAttr.append("description", QString::null, QString::null, i18n("Vertical multiplicator"));
        xmlAttr.append("max", QString::null, QString::null, "50");
        xmlAttr.append("min", QString::null, QString::null, "1");
        xmlAttr.append("default", QString::null, QString::null, "1");
        blur->addParameter(effectDescParamFactory.createParameter(xmlAttr));
        effectList->append(blur );
    }

    if (filtersList.findIndex("wave") != -1) {
        // wave
        EffectDesc *wave = new EffectDesc(i18n("Wave"), "wave", "wave", VIDEOEFFECT);
        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "double");
        xmlAttr.append("name", QString::null, QString::null, "start");
        xmlAttr.append("description", QString::null, QString::null, i18n("Amplitude"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "5");
	xmlAttr.append("factor", QString::null, QString::null, "1");
        wave->addParameter(effectDescParamFactory.createParameter(xmlAttr));

        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "deformX");
        xmlAttr.append("description", QString::null, QString::null, i18n("Horizontal factor"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "1");
        wave->addParameter(effectDescParamFactory.createParameter(xmlAttr));

        xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "deformY");
        xmlAttr.append("description", QString::null, QString::null, i18n("Vertical factor"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "1");
        wave->addParameter(effectDescParamFactory.createParameter(xmlAttr));
        effectList->append(wave );
    }

    if (filtersList.findIndex("sox") != -1) {

	// Pitch shift
	EffectDesc *pitch = new EffectDesc(i18n("Sox Pitch Shift"), "sox_pitch", "sox", AUDIOEFFECT);
	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "shift");
        xmlAttr.append("description", QString::null, QString::null, i18n("Shift"));
        xmlAttr.append("max", QString::null, QString::null, "1500");
        xmlAttr.append("min", QString::null, QString::null, "-1500");
        xmlAttr.append("default", QString::null, QString::null, "0");
        pitch->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "window");
        xmlAttr.append("description", QString::null, QString::null, i18n("Time window (ms)"));
        xmlAttr.append("max", QString::null, QString::null, "30");
        xmlAttr.append("min", QString::null, QString::null, "10");
        xmlAttr.append("default", QString::null, QString::null, "20");
        pitch->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(pitch);


	// Vibro
	EffectDesc *vibro = new EffectDesc(i18n("Sox Vibro"), "sox_vibro", "sox", AUDIOEFFECT);
	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "speed");
        xmlAttr.append("description", QString::null, QString::null, i18n("Speed"));
        xmlAttr.append("max", QString::null, QString::null, "30");
        xmlAttr.append("min", QString::null, QString::null, "1");
        xmlAttr.append("default", QString::null, QString::null, "15");
        vibro->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(vibro);

	// Flanger
	EffectDesc *flanger = new EffectDesc(i18n("Sox Flanger"), "sox_flanger", "sox", AUDIOEFFECT);
	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "gain-in");
        xmlAttr.append("description", QString::null, QString::null, i18n("Gain In"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "50");
        xmlAttr.append("factor", QString::null, QString::null, "100");
        flanger->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "gain-out");
        xmlAttr.append("description", QString::null, QString::null, i18n("Gain Out"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "50");
        xmlAttr.append("factor", QString::null, QString::null, "100");
        flanger->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "delay");
        xmlAttr.append("description", QString::null, QString::null, i18n("Delay"));
        xmlAttr.append("max", QString::null, QString::null, "500");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "100");
        xmlAttr.append("factor", QString::null, QString::null, "100");
        flanger->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "decay");
        xmlAttr.append("description", QString::null, QString::null, i18n("Decay"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "100");
        xmlAttr.append("factor", QString::null, QString::null, "100");
        flanger->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "speed");
        xmlAttr.append("description", QString::null, QString::null, i18n("Speed"));
        xmlAttr.append("max", QString::null, QString::null, "20");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "10");
        xmlAttr.append("factor", QString::null, QString::null, "10");
        flanger->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(flanger);

	// Reverb
	EffectDesc *reverb = new EffectDesc(i18n("Sox Reverb"), "sox_reverb", "sox", AUDIOEFFECT);
	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "gain-out");
        xmlAttr.append("description", QString::null, QString::null, i18n("Gain Out"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "50");
        xmlAttr.append("factor", QString::null, QString::null, "100");
        reverb->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "reverb-time");
        xmlAttr.append("description", QString::null, QString::null, i18n("Reverb Time"));
        xmlAttr.append("max", QString::null, QString::null, "5000");
        xmlAttr.append("min", QString::null, QString::null, "100");
        xmlAttr.append("default", QString::null, QString::null, "1000");
        reverb->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "delay");
        xmlAttr.append("description", QString::null, QString::null, i18n("Delay"));
        xmlAttr.append("max", QString::null, QString::null, "500");
        xmlAttr.append("min", QString::null, QString::null, "100");
        xmlAttr.append("default", QString::null, QString::null, "200");
        reverb->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(reverb);

	// Echo
	EffectDesc *chorus = new EffectDesc(i18n("Sox Echo"), "sox_echo", "sox", AUDIOEFFECT);
	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "gain-in");
        xmlAttr.append("description", QString::null, QString::null, i18n("Gain In"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "50");
        xmlAttr.append("factor", QString::null, QString::null, "100");
        chorus->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "gain-out");
        xmlAttr.append("description", QString::null, QString::null, i18n("Gain Out"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "100");
        xmlAttr.append("factor", QString::null, QString::null, "100");
        chorus->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "delay");
        xmlAttr.append("description", QString::null, QString::null, i18n("Delay"));
        xmlAttr.append("max", QString::null, QString::null, "500");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "150");
        chorus->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "constant");
        xmlAttr.append("name", QString::null, QString::null, "decay");
        xmlAttr.append("description", QString::null, QString::null, i18n("Decay"));
        xmlAttr.append("max", QString::null, QString::null, "100");
        xmlAttr.append("min", QString::null, QString::null, "0");
        xmlAttr.append("default", QString::null, QString::null, "80");
        xmlAttr.append("factor", QString::null, QString::null, "100");
        chorus->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(chorus);

    }

    if (filtersList.findIndex("volume") != -1) {
        // Audio volume
	EffectDesc *volume = new EffectDesc(i18n("Volume"), "volume", "volume", AUDIOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "double");
	xmlAttr.append("name", QString::null, QString::null, "gain");
	xmlAttr.append("description", QString::null, QString::null, i18n("Gain"));
	xmlAttr.append("starttag", QString::null, QString::null, "gain");
	xmlAttr.append("max", QString::null, QString::null, "300");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "100");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	volume->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(volume);
    
	// Audio muting
	EffectDesc *mute = new EffectDesc(i18n("Mute"), "mute", "volume", AUDIOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "gain");
	xmlAttr.append("max", QString::null, QString::null, "0");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "0");
	mute->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(mute);

	// Audio normalise
	EffectDesc *normalise = new EffectDesc(i18n("Normalise"), "normalise", "volume", AUDIOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "gain");
	xmlAttr.append("max", QString::null, QString::null, "0");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "normalise");
	normalise->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(normalise);
    }

    if (filtersList.findIndex("obscure") != -1) {
        // Region obscure
	EffectDesc *obscure = new EffectDesc(i18n("Obscure"), "obscure", "obscure", VIDEOEFFECT);

	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "complex");
	xmlAttr.append("name", QString::null, QString::null, i18n("X") + ";" + i18n("Y") + ";" + i18n("Width") + ";" + i18n("Height") + ";" + i18n("Averaging") + ";" + i18n("Autotrack") + ";" + i18n("Debug"));
	xmlAttr.append("min", QString::null, QString::null, "0;0;0;0;3");
	xmlAttr.append("max", QString::null, QString::null, QString::number(KdenliveSettings::defaultwidth()) + ";" + QString::number(KdenliveSettings::defaultheight()) + ";1000;1000;100;");
	xmlAttr.append("default", QString::null, QString::null, QString::number((int) KdenliveSettings::defaultwidth() / 2) + ";" + QString::number((int) KdenliveSettings::defaultheight() / 2) + ";100;100;20;0;0");
	obscure->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(obscure);
    }

    if (filtersList.findIndex("mirror") != -1) {
        // Mirror
	EffectDesc *mirror = new EffectDesc(i18n("Mirror"), "mirror", "mirror", VIDEOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "list");
	xmlAttr.append("paramlist", QString::null, QString::null, "horizontal,vertical,diagonal,xdiagonal,flip,flop");
	xmlAttr.append("default", QString::null, QString::null, "horizontal");
	xmlAttr.append("name", QString::null, QString::null, "mirror");
	xmlAttr.append("description", QString::null, QString::null,
	i18n("Mirroring direction"));
	mirror->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "bool");
	xmlAttr.append("name", QString::null, QString::null, "reverse");
	xmlAttr.append("description", QString::null, QString::null, i18n("Invert"));
	xmlAttr.append("default", QString::null, QString::null, "0");
	mirror->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	effectList->append(mirror);
    }


    if (producersList.findIndex("framebuffer") != -1) {
        // Slowmotion
	EffectDesc *slowmo = new EffectDesc(i18n("Speed"), "speed", "framebuffer", VIDEOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "_speed");
	xmlAttr.append("description", QString::null, QString::null, i18n("Speed"));
	xmlAttr.append("max", QString::null, QString::null, "300");
	xmlAttr.append("min", QString::null, QString::null, "1");
	xmlAttr.append("default", QString::null, QString::null, "100");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	slowmo->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "strobe");
	xmlAttr.append("description", QString::null, QString::null, i18n("Stroboscope"));
	xmlAttr.append("max", QString::null, QString::null, "100");
	xmlAttr.append("min", QString::null, QString::null, "1");
	xmlAttr.append("default", QString::null, QString::null, "1");
	slowmo->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "bool");
	xmlAttr.append("name", QString::null, QString::null, "reverse");
	xmlAttr.append("description", QString::null, QString::null, i18n("Reverse Playing"));
	xmlAttr.append("default", QString::null, QString::null, "0");
	slowmo->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(slowmo);


        // Freeze
	EffectDesc *freeze = new EffectDesc(i18n("Freeze"), "freeze", "framebuffer", VIDEOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "position");
	xmlAttr.append("name", QString::null, QString::null, "freeze");
	xmlAttr.append("description", QString::null, QString::null, i18n("Freeze at"));
	xmlAttr.append("max", QString::null, QString::null, "1000000");
	xmlAttr.append("min", QString::null, QString::null, "1");
	xmlAttr.append("default", QString::null, QString::null, "1");
	freeze->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "bool");
	xmlAttr.append("name", QString::null, QString::null, "freeze_before");
	xmlAttr.append("description", QString::null, QString::null, i18n("Freeze Before"));
	xmlAttr.append("default", QString::null, QString::null, "0");
	freeze->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "bool");
	xmlAttr.append("name", QString::null, QString::null, "freeze_after");
	xmlAttr.append("description", QString::null, QString::null, i18n("Freeze After"));
	xmlAttr.append("default", QString::null, QString::null, "0");
	freeze->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	effectList->append(freeze);
    }

    if (filtersList.findIndex("gamma") != -1) {
	// Gamma
	EffectDesc *gamma = new EffectDesc(i18n("Gamma"), "gamma", "gamma", VIDEOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "gamma");
	xmlAttr.append("description", QString::null, QString::null, i18n("Gamma"));
	xmlAttr.append("max", QString::null, QString::null, "300");
	xmlAttr.append("min", QString::null, QString::null, "1");
	xmlAttr.append("default", QString::null, QString::null, "100");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	gamma->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(gamma);
    }

    if (filtersList.findIndex("ladspa") != -1) {

	if (!locate("ladspa_plugin", "declip_1195.so").isEmpty()) {
	// Declipper
	EffectDesc *declip = new EffectDesc(i18n("Declipper"), "declipper", "ladspa1195", AUDIOEFFECT);
	xmlAttr.clear();
        xmlAttr.append("type", QString::null, QString::null, "fixed");
        declip->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(declip);
	}

	if (!locate("ladspa_plugin", "vynil_1905.so").isEmpty()) {
	// Vinyl
	EffectDesc *vinyl = new EffectDesc(i18n("Vinyl"), "vinyl", "ladspa1905", AUDIOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "year");
	xmlAttr.append("description", QString::null, QString::null, i18n("Year"));
	xmlAttr.append("max", QString::null, QString::null, "1990");
	xmlAttr.append("min", QString::null, QString::null, "1900");
	xmlAttr.append("default", QString::null, QString::null, "1990");
	xmlAttr.append("factor", QString::null, QString::null, "1");
	vinyl->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "rpm");
	xmlAttr.append("description", QString::null, QString::null, i18n("RPM"));
	xmlAttr.append("max", QString::null, QString::null, "78");
	xmlAttr.append("min", QString::null, QString::null, "33");
	xmlAttr.append("default", QString::null, QString::null, "33");
	xmlAttr.append("factor", QString::null, QString::null, "1");
	vinyl->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "warping");
	xmlAttr.append("description", QString::null, QString::null, i18n("Surface warping"));
	xmlAttr.append("max", QString::null, QString::null, "100");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "0");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	vinyl->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "crackle");
	xmlAttr.append("description", QString::null, QString::null, i18n("Crackle"));
	xmlAttr.append("max", QString::null, QString::null, "100");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "0");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	vinyl->addParameter(effectDescParamFactory.createParameter(xmlAttr));

	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "wear");
	xmlAttr.append("description", QString::null, QString::null, i18n("Wear"));
	xmlAttr.append("max", QString::null, QString::null, "100");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "0");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	vinyl->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(vinyl);
	}

	if (!locate("ladspa_plugin", "am_pitchshift_1433.so").isEmpty()) {
	// Pitch shifter
	EffectDesc *pitch = new EffectDesc(i18n("Pitch Shift"), "pitch_shift", "ladspa1433", AUDIOEFFECT, true);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "pitch");
	xmlAttr.append("description", QString::null, QString::null, i18n("Shift"));
	xmlAttr.append("max", QString::null, QString::null, "400");
	xmlAttr.append("min", QString::null, QString::null, "25");
	xmlAttr.append("default", QString::null, QString::null, "100");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	pitch->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(pitch);
	}

	if (!locate("ladspa_plugin", "gverb_1216.so").isEmpty()) {
	// Reverb
	EffectDesc *reverb = new EffectDesc(i18n("Room reverb"), "room_reverb", "ladspa1216", AUDIOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "room");
	xmlAttr.append("description", QString::null, QString::null, i18n("Room size (m)"));
	xmlAttr.append("max", QString::null, QString::null, "300");
	xmlAttr.append("min", QString::null, QString::null, "1");
	xmlAttr.append("default", QString::null, QString::null, "75");
	reverb->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "delay");
	xmlAttr.append("description", QString::null, QString::null, i18n("Delay (s/10)"));
	xmlAttr.append("max", QString::null, QString::null, "300");
	xmlAttr.append("min", QString::null, QString::null, "1");
	xmlAttr.append("default", QString::null, QString::null, "75");
	xmlAttr.append("factor", QString::null, QString::null, "10");
	reverb->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "damp");
	xmlAttr.append("description", QString::null, QString::null, i18n("Damping"));
	xmlAttr.append("max", QString::null, QString::null, "100");
	xmlAttr.append("min", QString::null, QString::null, "1");
	xmlAttr.append("default", QString::null, QString::null, "50");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	reverb->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	effectList->append(reverb);
	}

	if (!locate("ladspa_plugin", "plate_1423.so").isEmpty()) {
	// Reverb 2
	EffectDesc *reverb2 = new EffectDesc(i18n("Reverb"), "reverb", "ladspa1423", AUDIOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "time");
	xmlAttr.append("description", QString::null, QString::null, i18n("Reverb time"));
	xmlAttr.append("max", QString::null, QString::null, "85");
	xmlAttr.append("min", QString::null, QString::null, "1");
	xmlAttr.append("default", QString::null, QString::null, "42");
	xmlAttr.append("factor", QString::null, QString::null, "10");
	reverb2->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "damp");
	xmlAttr.append("description", QString::null, QString::null, i18n("Damping"));
	xmlAttr.append("max", QString::null, QString::null, "100");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "25");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	reverb2->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	effectList->append(reverb2);
	}

	if (!locate("ladspa_plugin", "dj_eq_1901.so").isEmpty()) {
	// Equalizer
	EffectDesc *equ = new EffectDesc(i18n("Equalizer"), "equalizer", "ladspa1901", AUDIOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "logain");
	xmlAttr.append("description", QString::null, QString::null, i18n("Lo gain"));
	xmlAttr.append("max", QString::null, QString::null, "6");
	xmlAttr.append("min", QString::null, QString::null, "-70");
	xmlAttr.append("default", QString::null, QString::null, "0");
	equ->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "midgain");
	xmlAttr.append("description", QString::null, QString::null, i18n("Mid gain"));
	xmlAttr.append("max", QString::null, QString::null, "6");
	xmlAttr.append("min", QString::null, QString::null, "-70");
	xmlAttr.append("default", QString::null, QString::null, "0");
	equ->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "higain");
	xmlAttr.append("description", QString::null, QString::null, i18n("Hi gain"));
	xmlAttr.append("max", QString::null, QString::null, "6");
	xmlAttr.append("min", QString::null, QString::null, "-70");
	xmlAttr.append("default", QString::null, QString::null, "0");
	equ->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	effectList->append(equ);
	}


	if (!locate("ladspa_plugin", "fast_lookahead_limiter_1913.so").isEmpty()) {
	// Limiter
	EffectDesc *limiter = new EffectDesc(i18n("Limiter"), "limiter", "ladspa1913", AUDIOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "gain");
	xmlAttr.append("description", QString::null, QString::null, i18n("Input Gain (db)"));
	xmlAttr.append("max", QString::null, QString::null, "20");
	xmlAttr.append("min", QString::null, QString::null, "-20");
	xmlAttr.append("default", QString::null, QString::null, "0");
	limiter->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "limit");
	xmlAttr.append("description", QString::null, QString::null, i18n("Limit (db)"));
	xmlAttr.append("max", QString::null, QString::null, "0");
	xmlAttr.append("min", QString::null, QString::null, "-20");
	xmlAttr.append("default", QString::null, QString::null, "0");
	limiter->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "release");
	xmlAttr.append("description", QString::null, QString::null, i18n("Release time (s)"));
	xmlAttr.append("max", QString::null, QString::null, "200");
	xmlAttr.append("min", QString::null, QString::null, "1");
	xmlAttr.append("default", QString::null, QString::null, "50");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	limiter->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	effectList->append(limiter);
	}

	if (!locate("ladspa_plugin", "pitch_scale_1193.so").isEmpty()) {
	// Pitch scaler
	EffectDesc *scaler = new EffectDesc(i18n("Pitch Scaler"), "pitch_scale", "ladspa1193", AUDIOEFFECT, true);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "coef");
	xmlAttr.append("description", QString::null, QString::null, i18n("Co-efficient"));
	xmlAttr.append("max", QString::null, QString::null, "200");
	xmlAttr.append("min", QString::null, QString::null, "50");
	xmlAttr.append("default", QString::null, QString::null, "100");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	scaler->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(scaler);
	}

	if (!locate("ladspa_plugin", "rate_shifter_1417.so").isEmpty()) {
	// Pitch scaler
	EffectDesc *scaler = new EffectDesc(i18n("Rate Scaler"), "rate_scale", "ladspa1417", AUDIOEFFECT, true);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "rate");
	xmlAttr.append("description", QString::null, QString::null, i18n("Rate"));
	xmlAttr.append("max", QString::null, QString::null, "40");
	xmlAttr.append("min", QString::null, QString::null, "-40");
	xmlAttr.append("default", QString::null, QString::null, "10");
	xmlAttr.append("factor", QString::null, QString::null, "10");
	scaler->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	effectList->append(scaler);
	}

	if (!locate("ladspa_plugin", "phasers_1217.so").isEmpty()) {
	// Phaser
	EffectDesc *phaser = new EffectDesc(i18n("Phaser"), "phaser", "ladspa1217", AUDIOEFFECT);
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "rate");
	xmlAttr.append("description", QString::null, QString::null, i18n("Rate (Hz)"));
	xmlAttr.append("max", QString::null, QString::null, "100");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "25");
	phaser->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "depth");
	xmlAttr.append("description", QString::null, QString::null, i18n("Depth"));
	xmlAttr.append("max", QString::null, QString::null, "100");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "25");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	phaser->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "feedback");
	xmlAttr.append("description", QString::null, QString::null, i18n("Feedback"));
	xmlAttr.append("max", QString::null, QString::null, "100");
	xmlAttr.append("min", QString::null, QString::null, "-100");
	xmlAttr.append("default", QString::null, QString::null, "0");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	phaser->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	xmlAttr.append("type", QString::null, QString::null, "constant");
	xmlAttr.append("name", QString::null, QString::null, "spread");
	xmlAttr.append("description", QString::null, QString::null, i18n("Spread"));
	xmlAttr.append("max", QString::null, QString::null, "200");
	xmlAttr.append("min", QString::null, QString::null, "0");
	xmlAttr.append("default", QString::null, QString::null, "100");
	xmlAttr.append("factor", QString::null, QString::null, "100");
	phaser->addParameter(effectDescParamFactory.createParameter(xmlAttr));
	xmlAttr.clear();
	effectList->append(phaser);
	}

    }
*/
}

//static 
char* initEffects::ladspaEffectString(int ladspaId, QStringList params)
{
    if (ladspaId == 1433 ) //Pitch
	return ladspaPitchEffectString(params);
    else if (ladspaId == 1216 ) //Room Reverb
	return ladspaRoomReverbEffectString(params);
    else if (ladspaId == 1423 ) //Reverb
	return ladspaReverbEffectString(params);
    else if (ladspaId == 1901 ) //Reverb
	return ladspaEqualizerEffectString(params);
    else {
	kdDebug()<<"++++++++++  ASKING FOR UNKNOWN LADSPA EFFECT: "<<ladspaId<<endl;
	return("<jackrack></jackrack>");
    }
}

//static 
void initEffects::ladspaEffectFile(const QString & fname, int ladspaId, QStringList params)
{
    char *filterString;
    switch (ladspaId) {
    case 1433: //Pitch
	filterString = ladspaPitchEffectString(params);
	break;
    case 1905: //Vinyl
	filterString = ladspaVinylEffectString(params);
	break;
    case 1216 : //Room Reverb
	filterString = ladspaRoomReverbEffectString(params);
	break;
    case 1423: //Reverb
	filterString = ladspaReverbEffectString(params);
	break;
    case 1195: //Declipper
	filterString = ladspaDeclipEffectString(params);
	break;
    case 1901:  //Reverb
	filterString = ladspaEqualizerEffectString(params);
	break;
    case 1913: // Limiter
	filterString = ladspaLimiterEffectString(params);
	break;
    case 1193: // Pitch Shifter
	filterString = ladspaPitchShifterEffectString(params);
	break;
    case 1417: // Rate Scaler
	filterString = ladspaRateScalerEffectString(params);
	break;
    case 1217: // Phaser
	filterString = ladspaPhaserEffectString(params);
	break;
    default: 
	kdDebug()<<"++++++++++  ASKING FOR UNKNOWN LADSPA EFFECT: "<<ladspaId<<endl;
	return;
	break;
    }

	QFile f( fname.ascii() );
    	if ( f.open( IO_WriteOnly ) ) 
	{
        	QTextStream stream( &f );
		stream << filterString;
		f.close();
    	}
    	else kdDebug()<<"++++++++++  ERROR CANNOT WRITE TO: "<<KdenliveSettings::currenttmpfolder() +  fname<<endl;
	delete filterString;
}

QString jackString = "<?xml version=\"1.0\"?><!DOCTYPE jackrack SYSTEM \"http://purge.bash.sh/~rah/jack_rack_1.2.dtd\"><jackrack><channels>2</channels><samplerate>48000</samplerate><plugin><id>";


char* initEffects::ladspaDeclipEffectString(QStringList)
{
	return KRender::decodedString( QString(jackString + "1195</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall></plugin></jackrack>"));
}

/*
char* initEffects::ladspaVocoderEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1441</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>0.000000</value><value>0.000000</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]));
}*/

char* initEffects::ladspaVinylEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1905</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow><controlrow><value>%3</value></controlrow><controlrow><value>%4</value></controlrow><controlrow><value>%5</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]).arg(params[4]));
}

char* initEffects::ladspaPitchEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1433</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.0</value><value>1.0</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>4.000000</value><value>4.000000</value></controlrow></plugin></jackrack>").arg(params[0]));
}

char* initEffects::ladspaRoomReverbEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1216</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>0.750000</value><value>0.750000</value></controlrow><controlrow><lock>true</lock><value>-70.000000</value><value>-70.000000</value></controlrow><controlrow><lock>true</lock><value>0.000000</value><value>0.000000</value></controlrow><controlrow><lock>true</lock><value>-17.500000</value><value>-17.500000</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]));
}

char* initEffects::ladspaReverbEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1423</id><enabled>true</enabled>  <wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked>    <wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values>    <lockall>true</lockall><controlrow><lock>true</lock><value>%1</value>      <value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>0.250000</value><value>0.250000</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]));
}

char* initEffects::ladspaEqualizerEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1901</id><enabled>true</enabled>    <wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked>    <wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow>    <controlrow><value>%3</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]));
}

char* initEffects::ladspaLimiterEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1913</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow><controlrow><value>%3</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]));
}

char* initEffects::ladspaPitchShifterEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1193</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow></plugin></jackrack>").arg(params[0]));
}

char* initEffects::ladspaRateScalerEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1417</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow></plugin></jackrack>").arg(params[0]));
}

char* initEffects::ladspaPhaserEffectString(QStringList params)
{
	return KRender::decodedString( QString(jackString + "1217</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]));
}



