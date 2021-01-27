/****************************************************************************
* MeshLab                                                           o o     *
* A versatile mesh processing toolbox                             o     o   *
*                                                                _   O  _   *
* Copyright(C) 2005-2020                                           \/)\/    *
* Visual Computing Lab                                            /\/|      *
* ISTI - Italian National Research Council                           |      *
*                                                                    \      *
* All rights reserved.                                                      *
*                                                                           *
* This program is free software; you can redistribute it and/or modify      *
* it under the terms of the GNU General Public License as published by      *
* the Free Software Foundation; either version 2 of the License, or         *
* (at your option) any later version.                                       *
*                                                                           *
* This program is distributed in the hope that it will be useful,           *
* but WITHOUT ANY WARRANTY; without even the implied warranty of            *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
* GNU General Public License (http://www.gnu.org/licenses/gpl.txt)          *
* for more details.                                                         *
*                                                                           *
****************************************************************************/

#include "plugin_manager.h"
#include <QObject>
#include <qapplication.h>
#include <QPluginLoader>
#include <QDebug>
#include <vcg/complex/algorithms/create/platonic.h>

#include "mlexception.h"
#include "globals/globals.h"

static QStringList fileNamePluginDLLs() {
	QStringList l;
#if defined(Q_OS_WIN)
	l.push_back("*.dll");
	return l;
#elif defined(Q_OS_MAC)
	l.push_back("*.dylib");
	l.push_back("*.so");
	return l;
#else //linux
	l.push_back("*.so");
	return l;
#endif
}

static QString fileNamePrefixPluginDLLs() {
#if defined(Q_OS_WIN)
	return QString("");
#else //macos and linux
	return QString("lib");
#endif
}

PluginManager::PluginManager()
{
}

PluginManager::PluginManager(RichParameterList& defaultGlobal)
{
	loadPlugins(defaultGlobal);
}

PluginManager::PluginManager(RichParameterList& defaultGlobal, const QDir& pluginsDirectory)
{
	loadPlugins(defaultGlobal, pluginsDirectory);
}

PluginManager::~PluginManager()
{
	ioMeshPlugins.clear();
	filterPlugins.clear();
	renderPlugins.clear();
	decoratePlugins.clear();
	for (auto& plugin : ownerPlug)
		delete plugin.second;
	ownerPlug.clear();
	
	for (int ii = 0; ii < editPlugins.size(); ++ii)
		delete editPlugins[ii];
	editPlugins.clear();
}



void PluginManager::loadPlugins(RichParameterList& defaultGlobal)
{
	loadPlugins(defaultGlobal, QDir(meshlab::defaultPluginPath()));
}

void PluginManager::loadPlugins(RichParameterList& defaultGlobal, const QDir& pluginsDirectory)
{
	pluginsDir = pluginsDirectory;
	// without adding the correct library path in the mac the loading of jpg (done via qt plugins) fails
	// ToDo: get rid of any qApp here
	qApp->addLibraryPath(meshlab::defaultPluginPath());
	QStringList nameFiltersPlugins = fileNamePluginDLLs();
	
	//only the file with extension pluginfilters will be listed by function entryList()
	pluginsDir.setNameFilters(nameFiltersPlugins);
	
	qDebug("Current Plugins Dir is: %s ", qUtf8Printable(pluginsDir.absolutePath()));
	for(QString fileName : pluginsDir.entryList(QDir::Files)) {
		QString absfilepath = pluginsDir.absoluteFilePath(fileName);
		QFileInfo fin(absfilepath);
		QPluginLoader loader(absfilepath);
		QObject *plugin = loader.instance();
		if (plugin) {
			pluginsLoaded.push_back(fileName);
			PluginInterface *iCommon = nullptr;
			FilterPluginInterface *iFilter = qobject_cast<FilterPluginInterface *>(plugin);
			if (iFilter)
			{
				iCommon = iFilter;
				bool loadFilterOK = true;
				for(QAction *filterAction : iFilter->actions()) {
					if(iFilter->getClass(filterAction)==FilterPluginInterface::Generic){
						qDebug() << "Missing class for " +fileName + " " + filterAction->text();
						loadFilterOK = false;
					}
					if(iFilter->getRequirements(filterAction) == int(MeshModel::MM_UNKNOWN)){
						qDebug() << "Missing requirements for " +fileName + " " + filterAction->text();
						loadFilterOK = false;
					}
					if(iFilter->getPreConditions(filterAction) == int(MeshModel::MM_UNKNOWN)){
						qDebug() << "Missing preconditions for "+fileName + " " + filterAction->text();
						loadFilterOK = false;
					}
					if(iFilter->postCondition(filterAction) == int(MeshModel::MM_UNKNOWN )) {
						qDebug() << "Missing postcondition for "+fileName + " " + filterAction->text();
						loadFilterOK = false;
					}
					if(iFilter->filterArity(filterAction) == FilterPluginInterface::UNKNOWN_ARITY ) {
						qDebug() << "Missing Arity for " +fileName + " " + filterAction->text();
						loadFilterOK = false;
					}
					
				}
				if (loadFilterOK) {
					for(QAction *filterAction : iFilter->actions()) {
						filterAction->setData(QVariant(fileName));
						actionFilterMap.insert(filterAction->text(), filterAction);
					}
					filterPlugins.push_back(iFilter);
				}
			}
			IOMeshPluginInterface *iIOMesh = qobject_cast<IOMeshPluginInterface *>(plugin);
			if (iIOMesh)
			{
				iCommon = iIOMesh;
				ioMeshPlugins.push_back(iIOMesh);
			}
			
			IORasterPluginInterface* iIORaster = qobject_cast<IORasterPluginInterface*>(plugin);
			if (iIORaster){
				iCommon = iIORaster;
				ioRasterPlugins.push_back(iIORaster);
			}
			
			DecoratePluginInterface *iDecorator = qobject_cast<DecoratePluginInterface *>(plugin);
			if (iDecorator)
			{
				iCommon = iDecorator;
				decoratePlugins.push_back(iDecorator);
				for(QAction *decoratorAction : iDecorator->actions())
				{
					decoratorActionList.push_back(decoratorAction);
					iDecorator->initGlobalParameterList(decoratorAction, defaultGlobal);
				}
			}
			
			RenderPluginInterface *iRender = qobject_cast<RenderPluginInterface *>(plugin);
			if (iRender)
			{
				iCommon = iRender;
				renderPlugins.push_back(iRender);
			}
			
			EditPluginInterfaceFactory *iEditFactory = qobject_cast<EditPluginInterfaceFactory *>(plugin);
			if (iEditFactory)
			{
				editPlugins.push_back(iEditFactory);
				for(QAction* editAction: iEditFactory->actions())
					editActionList.push_back(editAction);
			}
			else if (iCommon)
			{
				if (ownerPlug.find(iCommon->pluginName()) == ownerPlug.end()) {
					ownerPlug[iCommon->pluginName()] = iCommon;
				}
				else {
					std::cerr << "Warning: " << iCommon->pluginName().toStdString() << " has been already loaded.\n";
				}
			} else {
				// qDebug("Plugin %s was loaded, but could not be casted to any known type.", qUtf8Printable(fileName));
			}
		}
		else
			qDebug() << loader.errorString();
	}
	fillKnownIOFormats();
}

int PluginManager::numberIOPlugins() const
{
	return ioMeshPlugins.size();
}

unsigned int PluginManager::size() const
{
	return ownerPlug.size();
}

// Search among all the decorator plugins the one that contains a decoration with the given name
DecoratePluginInterface *PluginManager::getDecoratePlugin(const QString& name)
{
	for(DecoratePluginInterface *tt : decoratePlugins) {
		for( QAction *ac: tt->actions())
			if( name == tt->decorationName(ac) ) return tt;
	}
	assert(0);
	return 0;
}

QAction* PluginManager::filterAction(const QString& name)
{
	auto it = actionFilterMap.find(name);
	if (it != actionFilterMap.end())
		return it.value();
	else
		return nullptr;
}

PluginManager::PluginRangeIterator PluginManager::pluginIterator()
{
	return PluginRangeIterator(this);
}

PluginManager::FilterPluginRangeIterator PluginManager::filterPluginIterator()
{
	return FilterPluginRangeIterator(this);
}

PluginManager::RenderPluginRangeIterator PluginManager::renderPluginIterator()
{
	return RenderPluginRangeIterator(this);
}

PluginManager::DecoratePluginRangeIterator PluginManager::decoratePluginIterator()
{
	return DecoratePluginRangeIterator(this);
}

PluginManager::EditPluginFactoryRangeIterator PluginManager::editPluginFactoryIterator()
{
	return EditPluginFactoryRangeIterator(this);
}

void PluginManager::fillKnownIOFormats()
{
	QString allKnownFormatsFilter = QObject::tr("All known formats (");
	for (IOMeshPluginInterface* pMeshIOPlugin:  ioMeshPlugins) {
		allKnownFormatsFilter += addPluginMeshFormats(allKnowInputMeshFormats, inpMeshFilters, pMeshIOPlugin, pMeshIOPlugin->importFormats());
	}
	allKnownFormatsFilter.append(')');
	inpMeshFilters.push_front(allKnownFormatsFilter);
	
	for (IOMeshPluginInterface* pMeshIOPlugin:  ioMeshPlugins) {
		addPluginMeshFormats(allKnowOutputFormats, outFilters, pMeshIOPlugin, pMeshIOPlugin->exportFormats());
	}
	
	allKnownFormatsFilter = QObject::tr("All known formats (");
	
	for (IORasterPluginInterface* pRasterIOPlugin : ioRasterPlugins){
		allKnownFormatsFilter += addPluginRasterFormats(allKnownInputRasterFormats, inpRasterFilters, pRasterIOPlugin, pRasterIOPlugin->importFormats());
	}
	
	allKnownFormatsFilter.append(')');
	inpRasterFilters.push_front(allKnownFormatsFilter);
}

QString PluginManager::addPluginRasterFormats(
		QMap<QString, IORasterPluginInterface*>& map, 
		QStringList& formatFilters, 
		IORasterPluginInterface* pRasterIOPlugin,
		const QList<FileFormat>& format)
{
	QString allKnownFormatsFilter;
	for (const FileFormat& currentFormat : format) {
		QString currentFilterEntry = currentFormat.description + " (";
		
		//a particular file format could be associated with more than one file extension
		QStringListIterator itExtension(currentFormat.extensions);
		for (QString currentExtension : currentFormat.extensions) {
			currentExtension = currentExtension.toLower();
			if (!map.contains(currentExtension)) {
				map.insert(currentExtension, pRasterIOPlugin);
				allKnownFormatsFilter.append(QObject::tr(" *."));
				allKnownFormatsFilter.append(currentExtension);
			}
			currentFilterEntry.append(QObject::tr(" *."));
			currentFilterEntry.append(currentExtension);
		}
		currentFilterEntry.append(')');
		formatFilters.append(currentFilterEntry);
	}
	return allKnownFormatsFilter;
}

QString PluginManager::addPluginMeshFormats(
		QMap<QString, IOMeshPluginInterface*>& map, 
		QStringList& formatFilters, 
		IOMeshPluginInterface* pMeshIOPlugin,
		const QList<FileFormat>& format)
{
	QString allKnownFormatsFilter;
	for (const FileFormat& currentFormat : format) {
		QString currentFilterEntry = currentFormat.description + " (";
		
		//a particular file format could be associated with more than one file extension
		QStringListIterator itExtension(currentFormat.extensions);
		for (QString currentExtension : currentFormat.extensions) {
			currentExtension = currentExtension.toLower();
			if (!map.contains(currentExtension)) {
				map.insert(currentExtension, pMeshIOPlugin);
				allKnownFormatsFilter.append(QObject::tr(" *."));
				allKnownFormatsFilter.append(currentExtension);
			}
			currentFilterEntry.append(QObject::tr(" *."));
			currentFilterEntry.append(currentExtension);
		}
		currentFilterEntry.append(')');
		formatFilters.append(currentFilterEntry);
	}
	return allKnownFormatsFilter;
}

QString PluginManager::osIndependentPluginName(const QString& plname)
{
	QFileInfo fi(plname);
	QString res = fi.baseName();
	QString pref = fileNamePrefixPluginDLLs();
	return res.remove(0, pref.size());
}

