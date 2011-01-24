/**
* \file mainwindow.h
* 4/11/2009 LB Initial implementation
*
*/

#include "mainwindow.h"

// models
#include "GEOModels.h"
#include "PntsModel.h"
#include "LinesModel.h"
#include "StationTreeModel.h"
#include "MshModel.h"
#include "ConditionModel.h"

//dialogs
#include "DBConnectionDialog.h"
#include "DiagramPrefsDialog.h"
#include "GMSHPrefsDialog.h"
#include "ListPropertiesDialog.h"
#include "SHPImportDialog.h"
#include "VtkAddFilterDialog.h"
#include "VisPrefsDialog.h"

#include "OGSRaster.h"
#include "OGSError.h"
#include "Configure.h"
#include "VtkVisPipeline.h"
#include "VtkVisPipelineItem.h"
#include "RecentFiles.h"
#include "TreeModelIterator.h"
#include "VtkGeoImageSource.h"
#include "VtkBGImageSource.h"
#include "DatabaseConnection.h"

//test
#include "fem_ele.h"
#include "MeshQualityChecker.h"
// FileIO includes
#include "OGSIOVer4.h"
#include "StationIO.h"
#include "PetrelInterface.h"
#include "GocadInterface.h"
#include "XMLInterface.h"
#include "GMSHInterface.h"
#include "GMSInterface.h"
#include "NetCDFInterface.h"    //YW  07.2010

// Qt includes
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDesktopWidget>

// VTK includes
#include <vtkVRMLExporter.h>
#include <vtkOBJExporter.h>

#ifdef OGS_USE_OPENSG
#include <OpenSG/OSGSceneFileHandler.h>
#include <OpenSG/OSGCoredNodePtr.h>
#include <OpenSG/OSGGroup.h>
#include "vtkOsgActor.h"
#include "OsgWidget.h"
#endif

#ifdef OGS_USE_VRPN
	#include "TrackingSettingsWidget.h"
	#include "VtkTrackedCamera.h"
#endif // OGS_USE_VRPN


/// FEM. 11.03.2010. WW
#include "problem.h"
Problem *aproblem = NULL;

using namespace FileIO;

MainWindow::MainWindow(QWidget *parent /* = 0*/)
: QMainWindow(parent), _db (NULL)
{
    setupUi(this);

	// Setup connection GEOObjects to GUI through GEOModels and tab widgets
	_geoModels = new GEOModels();

	// station model
	stationTabWidget->treeView->setModel(_geoModels->getStationModel());
	connect(stationTabWidget->treeView, SIGNAL(stationListExportRequested(std::string, std::string)),
		this, SLOT(exportBoreholesToGMS(std::string, std::string)));	// export Stationlist to GMS
	connect(stationTabWidget->treeView, SIGNAL(stationListRemoved(std::string)),
		_geoModels, SLOT(removeStationVec(std::string)));	// update model when stations are removed
	connect(stationTabWidget->treeView, SIGNAL(stationListSaved(QString, QString)),
		this, SLOT(writeStationListToFile(QString, QString)));	// save Stationlist to File
	connect(_geoModels, SIGNAL(stationVectorRemoved(StationTreeModel*, std::string)),
		this, SLOT(updateDataViews()));						// update data view when stations are removed
	connect(stationTabWidget->treeView, SIGNAL(diagramRequested(QModelIndex&)),
		this, SLOT(showDiagramPrefsDialog(QModelIndex&)));		// connect treeview to diagramview

	// point models
	connect (_geoModels, SIGNAL(pointModelAdded(Model*)),
		pntTabWidget->dataViewWidget, SLOT(addModel(Model*)));
	connect(pntTabWidget->dataViewWidget, SIGNAL(requestModelClear(std::string)),
		_geoModels, SLOT(removePointVec(const std::string)));
	connect (_geoModels, SIGNAL(pointModelRemoved(Model*)),
		pntTabWidget->dataViewWidget, SLOT(removeModel(Model*)));
	connect(_geoModels, SIGNAL(pointModelRemoved(Model*)),
		this, SLOT(updateDataViews()));

	// line models
	connect (_geoModels, SIGNAL(polylineModelAdded(Model*)),
		lineTabWidget->dataViewWidget, SLOT(addModel(Model*)));
	connect(lineTabWidget->dataViewWidget, SIGNAL(requestModelClear(std::string)),
		_geoModels, SLOT(removePolylineVec(const std::string)));
	connect (_geoModels, SIGNAL(polylineModelRemoved(Model*)),
		lineTabWidget->dataViewWidget, SLOT(removeModel(Model*)));
	connect(_geoModels, SIGNAL(polylineModelRemoved(Model*)),
		this, SLOT(updateDataViews()));

	// surface models
	connect (_geoModels, SIGNAL(surfaceModelAdded(Model*)),
		surfaceTabWidget->dataViewWidget, SLOT(addModel(Model*)));
	connect(surfaceTabWidget->dataViewWidget, SIGNAL(requestModelClear(std::string)),
		_geoModels, SLOT(removeSurfaceVec(const std::string)));
	connect (_geoModels, SIGNAL(surfaceModelRemoved(Model*)),
		surfaceTabWidget->dataViewWidget, SLOT(removeModel(Model*)));
	connect(_geoModels, SIGNAL(surfaceModelRemoved(Model*)),
		this, SLOT(updateDataViews()));


	// Setup connections for mesh models to GUI
	_meshModels = new MshModel(_project);
	mshTabWidget->treeView->setModel(_meshModels);
	connect(mshTabWidget, SIGNAL(requestMeshRemoval(const QModelIndex&)),
		_meshModels, SLOT(removeMesh(const QModelIndex&)));

	// Setup connections for condition model to GUI
	_conditionModel = new ConditionModel(_project);
	conditionTabWidget->treeView->setModel(_conditionModel);
	connect(conditionTabWidget, SIGNAL(requestConditionRemoval(const QModelIndex&)),
		_conditionModel, SLOT(removeCondition(const QModelIndex&)));


	// vtk visualization pipeline
#ifdef OGS_USE_OPENSG
	OsgWidget* osgWidget = new OsgWidget(this, 0, Qt::Window);
	//osgWidget->show();
	osgWidget->sceneManager()->setRoot(makeCoredNode<OSG::Group>());
	osgWidget->sceneManager()->showAll();
	_vtkVisPipeline = new VtkVisPipeline(visualizationWidget->renderer(), osgWidget->sceneManager());
#else // OGS_USE_OPENSG
	_vtkVisPipeline = new VtkVisPipeline(visualizationWidget->renderer());
#endif // OGS_USE_OPENSG
	connect(_geoModels, SIGNAL(pointModelAdded(Model*)),
		_vtkVisPipeline, SLOT(addPipelineItem(Model*)));
	connect(_geoModels, SIGNAL(pointModelRemoved(Model*)),
		_vtkVisPipeline, SLOT(removeSourceItem(Model*)));
	connect(_geoModels, SIGNAL(polylineModelAdded(Model*)),
		_vtkVisPipeline, SLOT(addPipelineItem(Model*)));
	connect(_geoModels, SIGNAL(polylineModelRemoved(Model*)),
		_vtkVisPipeline, SLOT(removeSourceItem(Model*)));
	connect(_geoModels, SIGNAL(surfaceModelAdded(Model*)),
		_vtkVisPipeline, SLOT(addPipelineItem(Model*)));
	connect(_geoModels, SIGNAL(surfaceModelRemoved(Model*)),
		_vtkVisPipeline, SLOT(removeSourceItem(Model*)));
	connect(_geoModels, SIGNAL(stationVectorAdded(StationTreeModel*, std::string)),
		_vtkVisPipeline, SLOT(addPipelineItem(StationTreeModel*, std::string)));
	connect(_geoModels, SIGNAL(stationVectorRemoved(StationTreeModel*, std::string)),
		_vtkVisPipeline, SLOT(removeSourceItem(StationTreeModel*, std::string)));
	connect(_meshModels, SIGNAL(meshAdded(MshModel*, QModelIndex)),
		_vtkVisPipeline, SLOT(addPipelineItem(MshModel*,QModelIndex)));
	connect(_meshModels, SIGNAL(meshRemoved(MshModel*, QModelIndex)),
		_vtkVisPipeline, SLOT(removeSourceItem(MshModel*, QModelIndex)));

	connect(_vtkVisPipeline, SIGNAL(vtkVisPipelineChanged()),
		visualizationWidget->vtkWidget, SLOT(update()));
	connect(_vtkVisPipeline, SIGNAL(vtkVisPipelineChanged()),
		vtkVisTabWidget->vtkVisPipelineView, SLOT(expandAll()));

	vtkVisTabWidget->vtkVisPipelineView->setModel(_vtkVisPipeline);
	connect(vtkVisTabWidget->vtkVisPipelineView, SIGNAL(requestRemovePipelineItem(QModelIndex)),
		_vtkVisPipeline, SLOT(removePipelineItem(QModelIndex)));
	connect(vtkVisTabWidget->vtkVisPipelineView, SIGNAL(requestAddPipelineFilterItem(QModelIndex)),
		this, SLOT(showAddPipelineFilterItemDialog(QModelIndex)));
	connect(vtkVisTabWidget, SIGNAL(requestViewUpdate()),
		visualizationWidget, SLOT(updateView()));

	connect(vtkVisTabWidget->vtkVisPipelineView, SIGNAL(actorSelected(vtkProp3D*)),
		(QObject*)(visualizationWidget->interactorStyle()), SLOT(highlightActor(vtkProp3D*)));
	connect((QObject*)(visualizationWidget->vtkPickCallback()), SIGNAL(actorPicked(vtkProp3D*)),
		vtkVisTabWidget->vtkVisPipelineView, SLOT(selectItem(vtkProp3D*)));


	connect(vtkVisTabWidget->vtkVisPipelineView, SIGNAL(meshAdded(Mesh_Group::CFEMesh*, std::string&)),
		_meshModels, SLOT(addMesh(Mesh_Group::CFEMesh*, std::string&)));

	//TEST new ModelTest(_vtkVisPipeline, this);

	// Stack the data dock widgets together
	tabifyDockWidget(pntDock, lineDock);
	tabifyDockWidget(lineDock, stationDock);
	tabifyDockWidget(surfaceDock, stationDock);
	tabifyDockWidget(stationDock, mshDock);

	// Restore window geometry
	readSettings();

	// Get info on screens geometry(ies)
	_vtkWidget = visualizationWidget->vtkWidget;
	QDesktopWidget* desktopWidget = QApplication::desktop();
	#if OGS_QT_VERSION < 46
	const unsigned int screenCount = desktopWidget->numScreens();
	#else
	const unsigned int screenCount = desktopWidget->screenCount();
	#endif // OGS_QT_VERSION < 46
	for(size_t i = 0; i < screenCount; ++i)
		_screenGeometries.push_back(desktopWidget->availableGeometry(i));

	// Setup import files menu
	menu_File->insertMenu( action_Exit, createImportFilesMenu() );

	// Setup recent files menu
	RecentFiles* recentFiles = new RecentFiles(this, SLOT(openRecentFile()), "recentFileList", "OpenGeoSys-5");
	connect(this, SIGNAL(fileUsed(QString)), recentFiles, SLOT(setCurrentFile(QString)));
	menu_File->insertMenu( action_Exit, recentFiles->menu() );

	// Setup Windows menu
	QAction* showPntDockAction = pntDock->toggleViewAction();
	showPntDockAction->setStatusTip(tr("Shows / hides the points view"));
	connect(showPntDockAction, SIGNAL(triggered(bool)), this, SLOT(showPntDockWidget(bool)));
	menuWindows->addAction(showPntDockAction);

	QAction* showLineDockAction = lineDock->toggleViewAction();
	showLineDockAction->setStatusTip(tr("Shows / hides the lines view"));
	connect(showLineDockAction, SIGNAL(triggered(bool)), this, SLOT(showLineDockWidget(bool)));
	menuWindows->addAction(showLineDockAction);

	QAction* showStationDockAction = stationDock->toggleViewAction();
	showStationDockAction->setStatusTip(tr("Shows / hides the station view"));
	connect(showStationDockAction, SIGNAL(triggered(bool)), this, SLOT(showStationDockWidget(bool)));
	menuWindows->addAction(showStationDockAction);

	QAction* showSurfaceDockAction = surfaceDock->toggleViewAction();
	showSurfaceDockAction->setStatusTip(tr("Shows / hides the surface view"));
	connect(showSurfaceDockAction, SIGNAL(triggered(bool)), this, SLOT(showSurfaceDockWidget(bool)));
	menuWindows->addAction(showSurfaceDockAction);

	QAction* showMshDockAction = mshDock->toggleViewAction();
	showMshDockAction->setStatusTip(tr("Shows / hides the mesh view"));
	connect(showMshDockAction, SIGNAL(triggered(bool)), this, SLOT(showMshDockWidget(bool)));
	menuWindows->addAction(showMshDockAction);

	QAction* showCondDockAction = conditionDock->toggleViewAction();
	showCondDockAction->setStatusTip(tr("Shows / hides the mesh view"));
	connect(showCondDockAction, SIGNAL(triggered(bool)), this, SLOT(showMshDockWidget(bool)));
	menuWindows->addAction(showMshDockAction);

	QAction* showVisDockAction = vtkVisDock->toggleViewAction();
	showVisDockAction->setStatusTip(tr("Shows / hides the FEM Conditions view"));
	connect(showVisDockAction, SIGNAL(triggered(bool)), this, SLOT(showVisDockWidget(bool)));
	menuWindows->addAction(showVisDockAction);

	// Presentation mode
	QMenu* presentationMenu = new QMenu();
	presentationMenu->setTitle("Presentation on");
	connect( presentationMenu, SIGNAL(aboutToShow()), this, SLOT(createPresentationMenu()) );
	menuWindows->insertMenu(showVisDockAction, presentationMenu);

	_fileFinder.addDirectory(".");
	_fileFinder.addDirectory(std::string(SOURCEPATH).append("/FileIO"));

	#ifdef OGS_USE_VRPN
		VtkTrackedCamera* cam = static_cast<VtkTrackedCamera*>
			(visualizationWidget->renderer()->GetActiveCamera());
		_trackingSettingsWidget = new TrackingSettingsWidget(cam, visualizationWidget, Qt::Window);
	#endif // OGS_USE_VRPN


	// connects for point model
	//connect(pntTabWidget->pointsTableView, SIGNAL(itemSelectionChanged(const QItemSelection&,const QItemSelection&)),
	//	pntsModel, SLOT(setSelectionFromOutside(const QItemSelection&, const QItemSelection&)));
	//connect(pntTabWidget->clearAllPushButton, SIGNAL(clicked()), pntsModel, SLOT(clearData()));
	//connect(pntTabWidget->clearSelectedPushButton, SIGNAL(clicked()), pntsModel, SLOT(clearSelectedData()));
	//connect(pntTabWidget->clearAllPushButton, SIGNAL(clicked()), linesModel, SLOT(clearData()));

	// connects for station model
	connect(stationTabWidget->treeView, SIGNAL(propertiesDialogRequested(std::string)), this, SLOT(showPropertiesDialog(std::string)));
//	std::cout << "size of Point: " << sizeof (GEOLIB::Point) << std::endl;
//	std::cout << "size of CGLPoint: " << sizeof (CGLPoint) << std::endl;
//
//	std::cout << "size of Polyline: " << sizeof (GEOLIB::Polyline) << std::endl;
//	std::cout << "size of CGLPolyline: " << sizeof (CGLPolyline) << std::endl;
//
//	std::cout << "size of GEOLIB::Surface: " << sizeof (GEOLIB::Surface) << std::endl;
//	std::cout << "size of Surface: " << sizeof (Surface) << std::endl;
//
//	std::cout << "size of CCore: " << sizeof (Mesh_Group::CCore) << std::endl;
//	std::cout << "size of CNode: " << sizeof (Mesh_Group::CNode) << std::endl;
//	std::cout << "size of CElement: " << sizeof (Mesh_Group::CNode) << std::endl;
//	std::cout << "size of CEdge: " << sizeof (Mesh_Group::CEdge) << std::endl;
//	std::cout << "size of CFEMesh: " << sizeof (Mesh_Group::CFEMesh) << std::endl;
//	std::cout << "size of Matrix: " << sizeof (Math_Group::Matrix) << std::endl;
//
//	std::cout << "size of vec<size_t>: " << sizeof (Math_Group::vec<size_t>) << std::endl;
//	std::cout << "size of std::vector: " << sizeof (std::vector<size_t>) << std::endl;

//	std::cout << "size of CSourceTerm: " << sizeof (CSourceTerm) << std::endl;
//	std::cout << "size of CBoundaryCondition: " << sizeof (CBoundaryCondition) << std::endl;

	std::cout << "size of CElem: " << sizeof (CElem) << std::endl;
	std::cout << "size of CElement: " << sizeof (FiniteElement::CElement) << std::endl;
	std::cout << "size of CRFProcess: " << sizeof (CRFProcess) << std::endl;
	std::cout << "size of CFEMesh: " << sizeof (Mesh_Group::CFEMesh) << std::endl;
}

MainWindow::~MainWindow()
{
	delete _db;
	delete _vtkVisPipeline;
	delete _meshModels;
	delete _geoModels;

#ifdef OGS_USE_VRPN
	delete _trackingSettingsWidget;
#endif // OGS_USE_VRPN

}

void MainWindow::closeEvent( QCloseEvent* event )
{
	writeSettings();
	QWidget::closeEvent(event);
}

void MainWindow::showPntDockWidget( bool show )
{
	if (show)
		pntDock->show();
	else
		pntDock->hide();
}
void MainWindow::showLineDockWidget( bool show )
{
	if (show)
		lineDock->show();
	else
		lineDock->hide();
}

void MainWindow::showStationDockWidget( bool show )
{
	if (show)
		stationDock->show();
	else
		stationDock->hide();
}

void MainWindow::showSurfaceDockWidget( bool show )
{
	if (show)
		surfaceDock->show();
	else
		surfaceDock->hide();
}

void MainWindow::showMshDockWidget( bool show )
{
	if (show)
		mshDock->show();
	else
		mshDock->hide();
}

void MainWindow::showVisDockWidget( bool show )
{
	if (show)
		vtkVisDock->show();
	else
		vtkVisDock->hide();
}

void MainWindow::open()
{
	QSettings settings("UFZ", "OpenGeoSys-5");
    QString fileName = QFileDialog::getOpenFileName(this,
		"Select data file to open", settings.value("lastOpenedFileDirectory").toString(),
		"Geosys files (*.gsp *.gli *.gml *.msh *.stn *.cnd);;Project files (*.gsp);;GLI files (*.gli);;MSH files (*.msh);;STN files (*.stn);;All files (* *.*)");
     if (!fileName.isEmpty())
	 {
		QDir dir = QDir(fileName);
		settings.setValue("lastOpenedFileDirectory", dir.absolutePath());
		loadFile(fileName);
     }
}


void MainWindow::openDatabase()
{
	if (_db==NULL)
	{
		_db = new DatabaseConnection(_geoModels);
		_db->dbConnect();
	}

	if (_db!=NULL && _db->isConnected())
	{
		_db->getListSelection();
		updateDataViews();
	}
}

void MainWindow::openDatabaseConnection()
{
	if (_db==NULL)
		_db = new DatabaseConnection(_geoModels);
	DBConnectionDialog* dbConn = new DBConnectionDialog();
    connect(dbConn, SIGNAL(connectionRequested(QString, QString, QString, QString, QString)), _db, SLOT(setConnection(QString, QString, QString, QString, QString)));
    dbConn->show();
}


void MainWindow::openRecentFile()
{
	QAction* action = qobject_cast<QAction*>(sender());
	if (action)
		loadFile(action->data().toString());
}

void MainWindow::save()
{
	QSettings settings("UFZ", "OpenGeoSys-5");
	QStringList files = settings.value("recentFileList").toStringList();
	QString dir_str;
	if (files.size() != 0) dir_str = QFileInfo(files[0]).absolutePath();
	else dir_str = QDir::homePath();

	QString gliName = pntTabWidget->dataViewWidget->modelSelectComboBox->currentText();
	QString fileName = QFileDialog::getSaveFileName(this, "Save data as", dir_str,"GeoSys project (*.gsp);; Geosys geometry files (*.gml);;GeoSys4 geometry files (*.gli);;GMSH geometry files (*.geo)");

	if (!(fileName.isEmpty() || gliName.isEmpty()))
	{
		QFileInfo fi(fileName);

		if (fi.suffix().toLower() == "gsp")
		{
			std::string schemaName(_fileFinder.getPath("OpenGeoSysProject.xsd"));
			XMLInterface xml(_geoModels, schemaName);
			xml.writeProjectFile(fileName);
		}
		else if (fi.suffix().toLower() == "gml")
		{
			std::string schemaName(_fileFinder.getPath("OpenGeoSysGLI.xsd"));
			XMLInterface xml(_geoModels, schemaName);
			xml.writeGLIFile(fileName, gliName);
		}
		else if (fi.suffix().toLower() == "geo")
		{
			GMSHInterface gmsh_io (fileName.toStdString());
			std::vector<std::string> selected_geometries;
			const size_t param1 (2);
			const double param2 (0.3);
			const double param3 (0.05);
			gmsh_io.writeAllDataToGMSHInputFile(*_geoModels,
					selected_geometries, param1, param2, param3);
		}
		else if (fi.suffix().toLower() == "gli") {
//			writeGLIFileV4 (fileName.toStdString(), gliName.toStdString(), *_geoModels);
			writeAllDataToGLIFileV4 (fileName.toStdString(), *_geoModels);
		}

	}
	else if (!fileName.isEmpty() && gliName.isEmpty()) OGSError::box("No geometry data available.");
}

void MainWindow::loadFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly)) {
        QMessageBox::warning(this, tr("Application"),
                          tr("Cannot read file %1:\n%2.")
                          .arg(fileName)
                          .arg(file.errorString()));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QFileInfo fi(fileName);
    std::string base = fi.absoluteDir().absoluteFilePath(fi.completeBaseName()).toStdString();
    if (fi.suffix().toLower() == "gli") {
#ifndef NDEBUG
    	 QTime myTimer0;
    	 myTimer0.start();
#endif
//    	FileIO::readGLIFileV4 (fileName.toStdString(), _geoModels);
    	readGLIFileV4 (fileName.toStdString(), _geoModels);
#ifndef NDEBUG
    	std::cout << myTimer0.elapsed() << " ms" << std::endl;
#endif
//
//#ifndef NDEBUG
//    	QTime myTimer;
//    	myTimer.start();
//    	std::cout << "GEOLIB_Read_GeoLib ... " << std::flush;
//#endif
//    	GEOLIB_Read_GeoLib(base); //fileName.toStdString());
//        cout << "Nr. Points: " << gli_points_vector.size() << endl;
//		cout << "Nr. Lines: " << polyline_vector.size() << endl;
//		cout << "Nr. Surfaces: " << surface_vector.size() << endl;
//#ifndef NDEBUG
//    	 std::cout << myTimer.elapsed() << " ms" << std::endl;
//#endif
// 		GEOCalcPointMinMaxCoordinates();
    }
else if (fi.suffix().toLower() == "gsp")
	{
		std::string schemaName(_fileFinder.getPath("OpenGeoSysProject.xsd"));
		XMLInterface xml(_geoModels, schemaName);
		xml.readProjectFile(fileName);
	}
	else if (fi.suffix().toLower() == "gml")
	{
#ifndef NDEBUG
    	 QTime myTimer0;
    	 myTimer0.start();
#endif
		std::string schemaName(_fileFinder.getPath("OpenGeoSysGLI.xsd"));
		XMLInterface xml(_geoModels, schemaName);
		xml.readGLIFile(fileName);
#ifndef NDEBUG
    	std::cout << myTimer0.elapsed() << " ms" << std::endl;
#endif
	}
	// OpenGeoSys observation station files (incl. boreholes)
	else if (fi.suffix().toLower() == "stn")
	{
		std::string schemaName(_fileFinder.getPath("OpenGeoSysSTN.xsd"));
		XMLInterface xml(_geoModels, schemaName);
		xml.readSTNFile(fileName);
/*
		// old file reading routines for ascii files
		GEOLIB::Station::StationType type = GEOLIB::Station::BOREHOLE;
		vector<GEOLIB::Point*> *stations = new vector<GEOLIB::Point*>();
		string name;

		if (int returnValue = StationIO::readStationFile(fileName.toStdString(), name, stations, type))
		{
			if (returnValue<0) cout << "main(): An error occured while reading the file.\n";

			if (type == GEOLIB::Station::BOREHOLE)
			{
				QString filename = QFileDialog::getOpenFileName(this, tr("Open stratigraphy file"), "", tr("Station Files (*.stn)"));

				// read stratigraphy for all boreholes at once
				vector<GEOLIB::Point*> *boreholes = new vector<GEOLIB::Point*>();

				size_t vectorSize = stations->size();
				for (size_t i=0; i<vectorSize; i++) boreholes->push_back(static_cast<GEOLIB::StationBorehole*>(stations->at(i)));
				GEOLIB::StationBorehole::addStratigraphies(filename.toStdString(), boreholes);
				for (size_t i=0; i<vectorSize; i++) (*stations)[i] = (*boreholes)[i];
				delete boreholes;
//				/// *** read stratigraphy for each borehole seperately
//				for (int i=0; i<static_cast<int>(stations.size());i++)
//				{
//				StationBorehole* borehole = static_cast<StationBorehole*>(stations.at(i));
//				StationBorehole::addStratigraphy(filename.toStdString(), borehole);
//				if (borehole->find("q")) stations.at(i)->setColor(255,0,0);
//				}
			}

			_geoModels->addStationVec(stations, name, GEOLIB::getRandomColor());
		}
*/
	}
	// OpenGeoSys mesh files
    else if (fi.suffix().toLower() == "msh")
	{
		std::string name = fileName.toStdString();
		CFEMesh* msh = MshModel::loadMeshFromFile(name);
		if (msh) _meshModels->addMesh(msh, name);
		else OGSError::box("Failed to load a mesh file.");
	}
	// FEM condition files
	else if (fi.suffix().toLower() == "cnd")
	{
		std::string schemaName(_fileFinder.getPath("OpenGeoSysCond.xsd"));
		XMLInterface xml(_geoModels, schemaName);
		std::vector<FEMCondition*> conditions;
		xml.readFEMCondFile(conditions, fileName);
		if (!conditions.empty()) this->_conditionModel->addConditions(conditions);
	}

	// GMS borehole files
	else if (fi.suffix().toLower() == "txt")
	{
		std::vector<GEOLIB::Point*> *boreholes = new std::vector<GEOLIB::Point*>();
		std::string name = fi.baseName().toStdString();

		if (GMSInterface::readBoreholesFromGMS(boreholes, fileName.toStdString()))
			_geoModels->addStationVec(boreholes, name, GEOLIB::getRandomColor());
	}
	// GMS mesh files
	else if (fi.suffix().toLower() == "3dm")
	{
		std::string name = fileName.toStdString();
		CFEMesh* mesh = GMSInterface::readGMS3DMMesh(name);
		_meshModels->addMesh(mesh, name);
	}
	// goCAD files
	else if (fi.suffix().toLower() == "ts") {
#ifndef NDEBUG
    	 QTime myTimer;
    	 myTimer.start();
    	 std::cout << "GoCad Read ... " << std::flush;
#endif
    	 FileIO::GocadInterface (fileName.toStdString(), _geoModels);
#ifndef NDEBUG
    	 std::cout << myTimer.elapsed() << " ms" << std::endl;
#endif
	}

    // NetCDF files
	// YW  07.2010
	else if (fi.suffix().toLower() == "nc") {
#ifndef NDEBUG
    	 QTime myTimer;
    	 myTimer.start();
    	 std::cout << "NetCDF Read ...\n" << std::flush;
#endif
		 std::string name = fileName.toStdString();
         std::vector<GEOLIB::Point*> *pnt_vec = new std::vector<GEOLIB::Point*>();
		 /* Data dimensions. */
		 size_t len_rlat, len_rlon;
    	 FileIO::NetCDFInterface::readNetCDFData(name, pnt_vec, _geoModels, len_rlat, len_rlon);
		 CFEMesh* mesh = FileIO::NetCDFInterface::createMeshFromPoints(pnt_vec, len_rlat, len_rlon);
         //GridAdapter* grid = new GridAdapter(mesh);
         _meshModels->addMesh(mesh, name);
#ifndef NDEBUG
    	 std::cout << myTimer.elapsed() << " ms" << std::endl;
#endif
	}
	updateDataViews();

	emit fileUsed(fileName);
}

void MainWindow::loadPetrelFiles(const QStringList &sfc_file_names, const QStringList &well_path_file_names)
{
	QStringList::const_iterator it = sfc_file_names.begin();
	std::list<std::string> sfc_files;
	while(it != sfc_file_names.end()) {
		sfc_files.push_back ((*it).toStdString());
	    ++it;
	}

	it = well_path_file_names.begin();
	std::list<std::string> well_path_files;
	while(it != well_path_file_names.end()) {
		well_path_files.push_back ((*it).toStdString());
	    ++it;
	}

	std::string unique_str (*(sfc_files.begin()));

	PetrelInterface (sfc_files, well_path_files, unique_str, _geoModels);
}

void MainWindow::updateDataViews()
{
	visualizationWidget->showAll();
	pntTabWidget->		dataViewWidget->dataView->updateView();
	lineTabWidget->		dataViewWidget->dataView->updateView();
	surfaceTabWidget->	dataViewWidget->dataView->updateView();
	stationTabWidget->	treeView->updateView();
	mshTabWidget->		treeView->updateView();

    QApplication::restoreOverrideCursor();
}


void MainWindow::readSettings()
{
	QSettings settings("UFZ", "OpenGeoSys-5");

	restoreGeometry(settings.value("windowGeometry").toByteArray());
	restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::writeSettings()
{
	QSettings settings("UFZ", "OpenGeoSys-5");

	settings.setValue("windowGeometry", saveGeometry());
	settings.setValue("windowState", saveState());
}

void MainWindow::about()
{
	QString ogsVersion = QString(OGS_VERSION);
	QMessageBox::about(this, tr("About OpenGeoSys-5"), tr("Built on %1\nOGS Version: %2").
		arg(QDate::currentDate().toString()).arg(ogsVersion));
}

QMenu* MainWindow::createImportFilesMenu()
{
	QMenu* importFiles = new QMenu("&Import Files");
	QAction* gmsFiles = importFiles->addAction("GMS Files...");
	connect(gmsFiles, SIGNAL(triggered()), this, SLOT(importGMS()));
	QAction* gocadFiles = importFiles->addAction("Gocad Files...");
	QAction* netcdfFiles = importFiles->addAction("NetCDF Files...");
	connect(netcdfFiles, SIGNAL(triggered()), this, SLOT(importNetcdf()));
	connect(gocadFiles, SIGNAL(triggered()), this, SLOT(importGoCad()));
	QAction* petrelFiles = importFiles->addAction("Petrel Files...");
	connect(petrelFiles, SIGNAL(triggered()), this, SLOT(importPetrel()));
	QAction* rasterFiles = importFiles->addAction("&Raster Files...");
	connect(rasterFiles, SIGNAL(triggered()), this, SLOT(importRaster()));
	QAction* rasterPolyFiles = importFiles->addAction("Raster Files as PolyData...");
	connect(rasterPolyFiles, SIGNAL(triggered()), this, SLOT(importRasterAsPoly()));
	QAction* shapeFiles = importFiles->addAction("&Shape Files...");
	connect(shapeFiles, SIGNAL(triggered()), this, SLOT(importShape()));
	QAction* vtkFiles = importFiles->addAction("VTK Files...");
	connect( vtkFiles, SIGNAL(triggered()), this, SLOT(importVtk()) );

	return importFiles;
}

void MainWindow::importGMS()
{
	QSettings settings("UFZ", "OpenGeoSys-5");
    QString fileName = QFileDialog::getOpenFileName(this, "Select GMS file to import", settings.value("lastOpenedFileDirectory").toString(),"GMS files (*.txt *.3dm)");
     if (!fileName.isEmpty())
		{
			loadFile(fileName);
			QDir dir = QDir(fileName);
			settings.setValue("lastOpenedFileDirectory", dir.absolutePath());
		}
}

void MainWindow::importGoCad()
{
	QSettings settings("UFZ", "OpenGeoSys-5");
    QString fileName = QFileDialog::getOpenFileName(this,
		"Select data file to import", settings.value("lastOpenedFileDirectory").toString(),"Gocad files (*.ts);;Gocad lines (*.tline)");
     if (!fileName.isEmpty())
	{
        loadFile(fileName);
		QDir dir = QDir(fileName);
		settings.setValue("lastOpenedFileDirectory", dir.absolutePath());
     }
}

void MainWindow::importRaster()
{
	QSettings settings("UFZ", "OpenGeoSys-5");
	QString fileName = QFileDialog::getOpenFileName(this, "Select raster file to import", settings.value("lastOpenedFileDirectory").toString(),"Raster files (*.asc *.bmp *.jpg *.png *.tif);;");
	QFileInfo fi(fileName);
	QString fileType = fi.suffix().toLower();

	if ((fileType == "asc") ||
		(fileType == "tif") ||
		(fileType == "png") ||
		(fileType == "jpg") ||
		(fileType == "bmp"))
	{
		VtkGeoImageSource* geoImage = VtkGeoImageSource::New();
		geoImage->setImageFilename(fileName);
		_vtkVisPipeline->addPipelineItem(geoImage);

		QDir dir = QDir(fileName);
		settings.setValue("lastOpenedFileDirectory", dir.absolutePath());
		// create a 3d-quad-mesh from the image (this fails in CFEMesh::ConstructGrid() if the image is too large!)
//		std::string tmp (fi.baseName().toStdString());
//		 _meshModels->addMesh(GridAdapter::convertImgToMesh(raster, origin, scalingFactor), tmp);
	}
	//else if (fileType == "tif" || fileType == "tiff")
	//{
	//	vtkTIFFReader* imageSource = vtkTIFFReader::New();
	//	imageSource->SetFileName(fileName.toStdString().c_str());
	//	_vtkVisPipeline->addPipelineItem(imageSource);
	//}
	else if (fileName.length() > 0) OGSError::box("File extension not supported.");
}

void MainWindow::importRasterAsPoly()
{
	QSettings settings("UFZ", "OpenGeoSys-5");
	QString fileName = QFileDialog::getOpenFileName(this, "Select raster file to import", settings.value("lastOpenedFileDirectory").toString(),"Raster files (*.asc *.bmp *.jpg *.png *.tif);;");
	QFileInfo fi(fileName);

	if ((fi.suffix().toLower() == "asc") ||
		(fi.suffix().toLower() == "tif") ||
		(fi.suffix().toLower() == "png") ||
		(fi.suffix().toLower() == "jpg") ||
		(fi.suffix().toLower() == "bmp"))
	{
		QImage raster;
		QPointF origin;
		double scalingFactor;
		OGSRaster::loadImage(fileName, raster, origin, scalingFactor, true);

		VtkBGImageSource* bg = VtkBGImageSource::New();
			bg->SetOrigin(origin.x(), origin.y());
			bg->SetCellSize(scalingFactor);
			bg->SetRaster(raster);
			bg->SetName(fileName);
		_vtkVisPipeline->addPipelineItem(bg);

		QDir dir = QDir(fileName);
		settings.setValue("lastOpenedFileDirectory", dir.absolutePath());

	}
	else OGSError::box("File extension not supported.");
}

void MainWindow::importShape()
{
	QSettings settings("UFZ", "OpenGeoSys-5");
	QString fileName = QFileDialog::getOpenFileName(this, "Select shape file to import", settings.value("lastOpenedFileDirectory").toString(),"ESRI Shape files (*.shp );;");
//	QString fileName = QFileDialog::getOpenFileName(this, "Select shape file to import", "","ESRI Shape files (*.shp *.dbf);;");
	QFileInfo fi(fileName);

	if (fi.suffix().toLower() == "shp" || fi.suffix().toLower() == "dbf")
	{
		SHPImportDialog dlg( (fileName.toUtf8 ()).constData(), _geoModels);
		dlg.exec();

		QDir dir = QDir(fileName);
		settings.setValue("lastOpenedFileDirectory", dir.absolutePath());
	}
}

void MainWindow::importPetrel()
{
	QSettings settings("UFZ", "OpenGeoSys-5");
    QStringList sfc_file_names = QFileDialog::getOpenFileNames(this,
		"Select surface data file(s) to import", "","Petrel files (*)");
    QStringList well_path_file_names = QFileDialog::getOpenFileNames(this,
    		"Select well path data file(s) to import", "","Petrel files (*)");
    if (sfc_file_names.size() != 0 || well_path_file_names.size() != 0 )
	{
		loadPetrelFiles (sfc_file_names, well_path_file_names);
		QDir dir = QDir(sfc_file_names.at(0));
		settings.setValue("lastOpenedFileDirectory", dir.absolutePath());
	}
}

//YW  07.2010
void MainWindow::importNetcdf()
{
    QSettings settings("UFZ", "OpenGeoSys-5");
    QString fileName = QFileDialog::getOpenFileName(this,
		"Select NetCDF file to import", settings.value("lastOpenedFileDirectory").toString(), "NetCDF files (*.nc);;");
	if (!fileName.isEmpty())
	{
		loadFile(fileName);
		QDir dir = QDir(fileName);
		settings.setValue("lastOpenedFileDirectory", dir.absolutePath());
	}
}

void MainWindow::importVtk()
{
	QSettings settings("UFZ", "OpenGeoSys-5");
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
		"Select VTK file(s) to import", settings.value("lastOpenedFileDirectory").toString(), "VTK files (*.vtk *.vti *.vtr *.vts *.vtp *.vtu);;");
	foreach (QString fileName, fileNames)
	{
		if (!fileName.isEmpty())
		{
			_vtkVisPipeline->loadFromFile(fileName);
			QDir dir = QDir(fileName);
			settings.setValue("lastOpenedFileDirectory", dir.absolutePath());
		}
	}
}

void MainWindow::showPropertiesDialog(std::string name)
{
	ListPropertiesDialog dlg(name, _geoModels);
	connect(&dlg, SIGNAL(propertyBoundariesChanged(std::string, std::vector<PropertyBounds>)), _geoModels, SLOT(filterStationVec(std::string, std::vector<PropertyBounds>)));
	dlg.exec();
}

void MainWindow::showAddPipelineFilterItemDialog( QModelIndex parentIndex )
{
	VtkAddFilterDialog dlg(_vtkVisPipeline, parentIndex);
	dlg.exec();
}

void MainWindow::writeStationListToFile	(QString listName, QString fileName)
{
	std::string schemaName(_fileFinder.getPath("OpenGeoSysSTN.xsd"));
	XMLInterface xml(_geoModels, schemaName);
	xml.writeSTNFile(fileName, listName);
}

void MainWindow::exportBoreholesToGMS(std::string listName, std::string fileName)
{
	const std::vector<GEOLIB::Point*> *stations (_geoModels->getStationVec(listName));
	GMSInterface::writeBoreholesToGMS(stations, fileName);
}

void MainWindow::callGMSH(std::vector<std::string> const & selectedGeometries, size_t param1, double param2, double param3, double param4, bool delete_geo_file)
{
	if (!selectedGeometries.empty())
	{
		std::cout << "Start meshing..." << std::endl;

		QString fileName("");
		if (!delete_geo_file)
			 fileName = QFileDialog::getSaveFileName(this, "Save GMSH-file as", "","GMSH geometry files (*.geo)");
		else
			fileName = "tmp_gmsh.geo";

		if (param4 == -1) // adaptive meshing selected
		{
			GMSHInterface gmsh_io (fileName.toStdString()); // fname.toStdString());
			gmsh_io.writeAllDataToGMSHInputFile(*_geoModels, selectedGeometries, param1, param2, param3);
		}
		else // homogeneous meshing selected
		{
			// todo
		}

		if (delete_geo_file)
		{
			// delete tmp_gmsh.geo
		}
	}
	else
		std::cout << "No geometry information selected..." << std::endl;
}

void MainWindow::showDiagramPrefsDialog(QModelIndex &index)
{
	QString listName;
	GEOLIB::Station* stn = _geoModels->getStationModel()->stationFromIndex(index, listName);

	if (stn->type() == GEOLIB::Station::STATION)
	{
		DiagramPrefsDialog* prefs = new DiagramPrefsDialog(stn, listName, _db);
		prefs->show();
	}
	if (stn->type() == GEOLIB::Station::BOREHOLE)
		OGSError::box("No time series data available for borehole.");
}

void MainWindow::showGMSHPrefsDialog()
{
	GMSHPrefsDialog dlg(_geoModels);
	connect(&dlg, SIGNAL(requestMeshing(std::vector<std::string> const &, size_t, double, double, double, bool)),
		this, SLOT(callGMSH(std::vector<std::string> const &, size_t, double, double, double, bool)));
	dlg.exec();
}

void MainWindow::showVisalizationPrefsDialog()
{
	VisPrefsDialog dlg(_vtkVisPipeline);
	dlg.exec();
}

void MainWindow::showTrackingSettingsDialog()
{
	#ifdef OGS_USE_VRPN
	_trackingSettingsWidget->show();
	#else // OGS_USE_VRPN
	QMessageBox::warning(this, "Functionality not implemented", "Sorry but this progam was not compiled with VRPN support.");
	#endif // OGS_USE_VRPN
}


void MainWindow::ShowWindow()
{
	this->show();
}

void MainWindow::HideWindow()
{
	this->hide();
}


void MainWindow::on_actionExportVTK_triggered( bool checked /*= false*/ )
{
	Q_UNUSED(checked)
	QSettings settings("UFZ", "OpenGeoSys-5");
	int count = 0;
	QString filename = QFileDialog::getSaveFileName(this, "Export object to vtk-files", settings.value(
				"lastExportedFileDirectory").toString(),"VTK files (*.vtp *.vtu)");
	if (!filename.isEmpty())
	{
		QDir dir = QDir(filename);
		settings.setValue("lastExportedFileDirectory", dir.absolutePath());

		std::string basename = QFileInfo(filename).path().toStdString();
		basename.append("/" + QFileInfo(filename).baseName().toStdString());
		TreeModelIterator it(_vtkVisPipeline);
		++it;
		while(*it)
		{
			count++;
			static_cast<VtkVisPipelineItem*>(*it)->writeToFile(basename + number2str(count));
			++it;
		}
	}
}

void MainWindow::on_actionExportVRML2_triggered( bool checked /*= false*/ )
{
	Q_UNUSED(checked)
	QSettings settings("UFZ", "OpenGeoSys-5");
	QString fileName = QFileDialog::getSaveFileName(this, "Save scene to VRML file", settings.value(
				"lastExportedFileDirectory").toString(),"VRML files (*.wrl);;");
	if(!fileName.isEmpty())
	{
		QDir dir = QDir(fileName);
		settings.setValue("lastExportedFileDirectory", dir.absolutePath());

		vtkVRMLExporter* exporter = vtkVRMLExporter::New();
		exporter->SetFileName(fileName.toStdString().c_str());
		exporter->SetRenderWindow(visualizationWidget->vtkWidget->GetRenderWindow());
		exporter->Write();
		exporter->Delete();
	}
}

void MainWindow::on_actionExportObj_triggered( bool checked /*= false*/ )
{
	Q_UNUSED(checked)
	QSettings settings("UFZ", "OpenGeoSys-5");
	QString fileName = QFileDialog::getSaveFileName(this, "Save scene to Wavefront OBJ files", settings.value(
				"lastExportedFileDirectory").toString(),";;");
	if(!fileName.isEmpty())
	{
		QDir dir = QDir(fileName);
		settings.setValue("lastExportedFileDirectory", dir.absolutePath());

		vtkOBJExporter* exporter = vtkOBJExporter::New();
		exporter->SetFilePrefix(fileName.toStdString().c_str());
		exporter->SetRenderWindow(visualizationWidget->vtkWidget->GetRenderWindow());
		exporter->Write();
		exporter->Delete();
	}
}

void MainWindow::on_actionExportOpenSG_triggered(bool checked /*= false*/ )
{
	Q_UNUSED(checked)
#ifdef OGS_USE_OPENSG
	QSettings settings("UFZ", "OpenGeoSys-5");
	QString filename = QFileDialog::getSaveFileName(
		this, "Export scene to OpenSG binary file",	settings.value(
			"lastExportedFileDirectory").toString(), "OpenSG files (*.osb);;");
	if (!filename.isEmpty())
	{
		QDir dir = QDir(filename);
		settings.setValue("lastExportedFileDirectory", dir.absolutePath());

		TreeModelIterator it(_vtkVisPipeline);
		++it;
		OSG::NodePtr root = OSG::makeCoredNode<OSG::Group>();
		while(*it)
		{
			VtkVisPipelineItem* item = static_cast<VtkVisPipelineItem*>(*it);
			vtkOsgActor* actor = static_cast<vtkOsgActor*>(item->actor());
			actor->SetVerbose(true);
			actor->UpdateOsg();
			beginEditCP(root);
			root->addChild(actor->GetOsgRoot());
			endEditCP(root);
			actor->ClearOsg();
			++it;
		}

		OSG::SceneFileHandler::the().write(root, filename.toStdString().c_str());
	}
#else
	QMessageBox::warning(this, "Functionality not implemented", "Sorry but this progam was not compiled with OpenSG support.");
#endif
}

void MainWindow::createPresentationMenu()
{
	QMenu* menu = static_cast<QMenu*>(QObject::sender());
	menu->clear();
	if (!_vtkWidget->parent())
	{
		QAction* action = new QAction("Quit presentation mode", menu);
		connect(action, SIGNAL(triggered()), this, SLOT(quitPresentationMode()));
		action->setShortcutContext(Qt::WidgetShortcut);
		action->setShortcut(QKeySequence(Qt::Key_Escape));
		menu->addAction(action);
	}
	else
	{
		int count = 0;
		const int currentScreen = QApplication::desktop()->screenNumber(visualizationWidget);
		foreach (QRect screenGeo, _screenGeometries)
		{
			Q_UNUSED(screenGeo);
			QAction* action = new QAction(QString("On screen %1").arg(count), menu);
			connect( action, SIGNAL(triggered()), this, SLOT(startPresentationMode()) );
			if (count == currentScreen)
				action->setEnabled(false);
			menu->addAction(action);
			++count;
		}
	}
}

void MainWindow::startPresentationMode()
{
	// Save the QMainWindow state to restore when quitting presentation mode
	_windowState = this->saveState();

	// Get the screen number from the QAction which sent the signal
	QString actionText = static_cast<QAction*>(QObject::sender())->text();
	int screen = actionText.split(" ").back().toInt();

	// Move the widget to the screen and maximize it
	// Real fullscreen hides the menu
	_vtkWidget->setParent(NULL, Qt::Window);
	_vtkWidget->move(QPoint(_screenGeometries[screen].x(), _screenGeometries[screen].y()));
	//_vtkWidget->showFullScreen();
	_vtkWidget->showMaximized();

	// Create an action which quits the presentation mode when pressing
	// ESCAPE when the the window has focus
	QAction* action = new QAction("Quit presentation mode", this);
	connect(action, SIGNAL(triggered()), this, SLOT(quitPresentationMode()));
	action->setShortcutContext(Qt::WidgetShortcut);
	action->setShortcut(QKeySequence(Qt::Key_Escape));
	_vtkWidget->addAction(action);

	// Hide the central widget to maximize the dock widgets
	QMainWindow::centralWidget()->hide();
}

void MainWindow::quitPresentationMode()
{
	// Remove the quit action
	QAction* action = _vtkWidget->actions().back();
	_vtkWidget->removeAction(action);
	delete action;

	// Add the widget back to visualization widget
	visualizationWidget->layout()->addWidget(_vtkWidget);

	QMainWindow::centralWidget()->show();

	// Restore the previously saved QMainWindow state
	this->restoreState(_windowState);
}
