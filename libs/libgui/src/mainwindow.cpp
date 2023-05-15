/*
# PostgreSQL Database Modeler (pgModeler)
#
# Copyright 2006-2023 - Raphael Araújo e Silva <raphael@pgmodeler.io>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# The complete text of GPLv3 is at LICENSE file on source code root directory.
# Also, you can get the complete GNU General Public License at <http://www.gnu.org/licenses/>
*/

#include "mainwindow.h"
#include "guiutilsns.h"
#include "tools/bugreportform.h"
#include "tools/metadatahandlingform.h"
#include "tools/sqlexecutionwidget.h"
#include "utilsns.h"

bool MainWindow::confirm_validation=true;
int MainWindow::ToolsActionsCount=0;

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags) : QMainWindow(parent, flags)
{
	setupUi(this);
	setAcceptDrops(true);

	pending_op = NoPendingOp;
	welcome_wgt = nullptr;
	window_title = this->windowTitle() + " " + GlobalAttributes::PgModelerVersion;

	recent_models_menu = new QMenu(this);
	recent_models_menu->setObjectName("recent_models_menu");

	#ifdef DEMO_VERSION
		window_title+=tr(" (Demo)");
	#endif

	setWindowTitle(window_title);
	createMainWidgets();
	loadConfigurations();
	configureMenusActionsWidgets();
	connectSignalsToSlots();
	showRightWidgetsBar();
	showBottomWidgetsBar();
	updateConnections();
	updateRecentModelsMenu();
	configureSamplesMenu();
	applyConfigurations();

	try
	{
		SQLExecutionWidget::loadSQLHistory();
	}
	catch(Exception &){}

	GeneralConfigWidget *conf_wgt=dynamic_cast<GeneralConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::GeneralConfWgt));
	std::map<QString, attribs_map >confs = conf_wgt->getConfigurationParams();

	//Restoring the canvas grid options
	action_show_grid->setChecked(confs[Attributes::Configuration][Attributes::ShowCanvasGrid]==Attributes::True);
	action_alin_objs_grade->setChecked(confs[Attributes::Configuration][Attributes::AlignObjsToGrid]==Attributes::True);
	action_show_delimiters->setChecked(confs[Attributes::Configuration][Attributes::ShowPageDelimiters]==Attributes::True);
	action_lock_delim->setChecked(confs[Attributes::Configuration][Attributes::LockPageDelimResize]==Attributes::True);
	action_compact_view->setChecked(confs[Attributes::Configuration][Attributes::CompactView]==Attributes::True);

	ObjectsScene::setShowGrid(action_show_grid->isChecked());
	ObjectsScene::setShowPageDelimiters(action_show_delimiters->isChecked());
	ObjectsScene::setAlignObjectsToGrid(action_alin_objs_grade->isChecked());

	#ifndef Q_OS_MAC
		//Hiding/showing the main menu bar depending on the retrieved conf
		bool show_main_menu = confs[Attributes::Configuration][Attributes::ShowMainMenu]==Attributes::True;
		main_menu_mb->setVisible(show_main_menu);

		if(show_main_menu)
			file_menu->addAction(action_hide_main_menu);

		main_menu.menuAction()->setVisible(!show_main_menu);
	#endif

	restoreDockWidgetsSettings();

	//Positioning the update notifier widget before showing it (if there is an update)
	setFloatingWidgetPos(update_notifier_wgt, action_update_found, model_acts_tb, false);
	action_update_found->setVisible(false);
	QTimer::singleShot(1000, this, &MainWindow::restoreTemporaryModels);

	//If there's no previuos geometry registered for the mainwindow display it maximized
	if(!GeneralConfigWidget::restoreWidgetGeometry(this))
		this->setWindowState(Qt::WindowMaximized);

	#ifdef NO_UPDATE_CHECK
		#warning "NO UPDATE CHECK: Update checking is disabled."
	#else
		//Enabling update check at startup
		if(confs[Attributes::Configuration][Attributes::CheckUpdate]==Attributes::True)
		{
			update_notifier_wgt->setCheckVersions(confs[Attributes::Configuration][Attributes::CheckVersions]);
			QTimer::singleShot(15000, update_notifier_wgt, &UpdateNotifierWidget::checkForUpdate);
		}
	#endif

	#ifdef DEMO_VERSION
		#warning "DEMO VERSION: demonstration version startup alert."
		QTimer::singleShot(5000, this, [this](){
			showDemoVersionWarning();
		});
	#endif

	#ifdef CHECK_CURR_VER
		//Showing the donate widget in the first run or if the version registered in the file diverges from the current
		if(confs[Attributes::Configuration][Attributes::FirstRun] != Attributes::False ||
			 confs[Attributes::Configuration][Attributes::PgModelerVersion] != GlobalAttributes::PgModelerVersion)
			QTimer::singleShot(1000, action_donate, &QAction::trigger);
	#endif

	// Post initilize plug-ins
	PluginsConfigWidget *plugins_conf_wgt = dynamic_cast<PluginsConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::PluginsConfWgt));
	plugins_conf_wgt->postInitPlugins();

	// Updating drop shadows settings to match the current UI theme
	AppearanceConfigWidget *appearance_wgt = dynamic_cast<AppearanceConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::AppearanceConfWgt));
	appearance_wgt->updateDropShadows();
}

MainWindow::~MainWindow()
{
	//This fix the crash on exit at Mac OSX system (but not sure why) (???)
	file_menu->clear();

	delete restoration_form;
	delete overview_wgt;
	delete configuration_form;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if(!event->mimeData()->hasUrls())
	{
		event->ignore();
		return;
	}

	for(auto &url : event->mimeData()->urls())
	{
		if(!url.toLocalFile().endsWith(GlobalAttributes::DbModelExt, Qt::CaseInsensitive))
		{
			event->ignore();
			return;
		}
	}

	event->accept();
}

void MainWindow::dropEvent(QDropEvent *event)
{
	if(event->mimeData()->hasUrls())
	{
		QStringList dbm_files;
		QString file;

		for(auto &url : event->mimeData()->urls())
		{
			file = url.toLocalFile();

			if(file.endsWith(GlobalAttributes::DbModelExt, Qt::CaseInsensitive))
				dbm_files.append(file);
		}

		loadModels(dbm_files);
	}
}

void MainWindow::configureMenusActionsWidgets()
{
	QAction *act_more = more_actions_menu.menuAction();
	act_more->setText(tr("More"));
	act_more->setIcon(QIcon(GuiUtilsNs::getIconPath("moreactions")));
	act_more->setToolTip(tr("Additional actions over the model"));

	fix_menu.addAction(action_fix_model);
	fix_menu.addAction(action_handle_metadata);

	QAction *act_fix = fix_menu.menuAction();
	act_fix->setIcon(QIcon(GuiUtilsNs::getIconPath("fix")));
	act_fix->setText(tr("Fix"));
	tools_acts_tb->insertAction(action_configuration, fix_menu.menuAction());
	QToolButton *tool_btn = qobject_cast<QToolButton *>(tools_acts_tb->widgetForAction(fix_menu.menuAction()));
	tool_btn->setPopupMode(QToolButton::InstantPopup);

	QAction *act_arrange_objs = show_menu->insertMenu(action_compact_view, &arrange_menu);
	act_arrange_objs->setText(tr("Arrange objects"));
	act_arrange_objs->setToolTip(tr("Rearrange objects over the canvas"));
	act_arrange_objs->setIcon(QIcon(GuiUtilsNs::getIconPath("arrangetables")));
	act_arrange_objs->setEnabled(false);
	model_acts_tb->insertAction(action_compact_view, act_arrange_objs);

	tool_btn = qobject_cast<QToolButton *>(model_acts_tb->widgetForAction(act_arrange_objs));
	tool_btn->setPopupMode(QToolButton::InstantPopup);

	arrange_menu.addAction(tr("Grid"), this, &MainWindow::arrangeObjects);
	arrange_menu.addAction(tr("Hierarchical"), this, &MainWindow::arrangeObjects);
	arrange_menu.addAction(tr("Scattered"), this, &MainWindow::arrangeObjects);

	models_tbw->tabBar()->setVisible(false);
	action_welcome->setData(WelcomeView);
	action_design->setData(DesignView);
	action_manage->setData(ManageView);

	//Enables the action to restore session when there are registered session files
	action_restore_session->setEnabled(!prev_session_files.isEmpty());
	welcome_wgt->last_session_tb->setEnabled(action_restore_session->isEnabled());

	this->setFocusPolicy(Qt::WheelFocus);
	model_acts_tb->addWidget(model_nav_wgt);
	model_acts_tb->addSeparator();

	QAction *act_plugins_conf = plugins_config_menu->menuAction();
	model_acts_tb->addAction(plugins_config_menu->menuAction());
	QToolButton *plugins_btn = dynamic_cast<QToolButton *>(model_acts_tb->widgetForAction(plugins_config_menu->menuAction()));
	plugins_btn->setPopupMode(QToolButton::InstantPopup);
	plugins_btn->setIcon(QIcon(GuiUtilsNs::getIconPath("pluginsconfig")));
	plugins_btn->setToolButtonStyle(Qt::ToolButtonIconOnly);

	if(!plugins_tb_acts.isEmpty())
	{
		for(auto &act : plugins_tb_acts)
		{
			model_acts_tb->insertAction(act_plugins_conf, act);
			dynamic_cast<QToolButton *>(model_acts_tb->widgetForAction(act))->setPopupMode(QToolButton::InstantPopup);
		}

		model_acts_tb->addSeparator();
	}

	model_acts_tb->addAction(action_bug_report);
	model_acts_tb->addAction(action_donate);
	model_acts_tb->addAction(action_support);
	model_acts_tb->addAction(action_about);
	model_acts_tb->addAction(action_update_found);

	current_model=nullptr;
	models_tbw->setVisible(false);
	model_objs_parent->setVisible(false);
	oper_list_parent->setVisible(false);
	obj_finder_parent->setVisible(false);
	model_valid_parent->setVisible(false);
	bg_saving_wgt->setVisible(false);
	about_wgt->setVisible(false);
	donate_wgt->setVisible(false);
	models_tbw_parent->lower();
	welcome_wgt->lower();
	v_splitter1->lower();

	QVBoxLayout *vlayout=new QVBoxLayout;
	vlayout->setContentsMargins(0,0,0,0);
	vlayout->addWidget(model_objs_wgt);
	model_objs_parent->setLayout(vlayout);

	vlayout=new QVBoxLayout;
	vlayout->setContentsMargins(0,0,0,0);
	vlayout->addWidget(oper_list_wgt);
	oper_list_parent->setLayout(vlayout);

	QHBoxLayout * hlayout=new QHBoxLayout;
	hlayout->setContentsMargins(0,0,0,0);
	hlayout->addWidget(model_valid_wgt);
	model_valid_parent->setLayout(hlayout);

	hlayout=new QHBoxLayout;
	hlayout->setContentsMargins(0,0,0,0);
	hlayout->addWidget(obj_finder_wgt);
	obj_finder_parent->setLayout(hlayout);

	models_tbw_parent->resize(QSize(models_tbw_parent->maximumWidth(), models_tbw_parent->height()));

	//Forcing the splitter that handles the bottom widgets to resize its children to their minimum size
	v_splitter1->setSizes({500, 250, 500});

#ifdef Q_OS_MAC
//	model_acts_tb->removeAction(action_main_menu);
//	action_main_menu->setEnabled(false);
#else
	plugins_config_menu->menuAction()->setIconVisibleInMenu(false);
	main_menu.addMenu(file_menu);
	main_menu.addMenu(edit_menu);
	main_menu.addMenu(show_menu);
	main_menu.addMenu(plugins_config_menu);
	main_menu.addMenu(about_menu);
	main_menu.addSeparator();
	main_menu.addAction(action_show_main_menu);

	main_menu.setIcon(QIcon(GuiUtilsNs::getIconPath("mainmenu")));
	main_menu.menuAction()->setText(tr("Main menu"));
	main_menu.menuAction()->setToolTip(tr("Main menu"));
	model_acts_tb->insertAction(action_new_model, main_menu.menuAction());
	dynamic_cast<QToolButton *>(model_acts_tb->widgetForAction(main_menu.menuAction()))->setPopupMode(QToolButton::InstantPopup);
#endif

	QAction *act = recent_models_menu->menuAction();
	act->setIcon(QIcon(GuiUtilsNs::getIconPath("loadrecent")));
	act->setText(tr("Recent models"));
	act->setToolTip(tr("Load recently opened model"));
	model_acts_tb->insertAction(action_load_model, act);
	dynamic_cast<QToolButton *>(model_acts_tb->widgetForAction(act))->setPopupMode(QToolButton::InstantPopup);

	QToolButton *btn = nullptr;
	QFont font;

	for(auto &act : tools_acts_tb->actions())
	{
		btn = qobject_cast<QToolButton *>(tools_acts_tb->widgetForAction(act));

		if(btn)
		{
			font = btn->font();
			font.setWeight(QFont::Normal);
			btn->setFont(font);
			GuiUtilsNs::createDropShadow(btn, 1, 1, 5);
		}
	}

	for(auto &act : model_acts_tb->actions())
	{
		btn = qobject_cast<QToolButton *>(model_acts_tb->widgetForAction(act));

		if(btn)
			GuiUtilsNs::createDropShadow(btn, 1, 1, 5);
	}

	ToolsActionsCount = tools_acts_tb->actions().size();
	QList<QAction *> actions = model_acts_tb->actions();
	actions.append(tools_acts_tb->actions());

	for(auto &act : actions)
	{
		if(!act->shortcut().toString().isEmpty())
			act->setToolTip(act->toolTip() + QString(" (%1)").arg(act->shortcut().toString()));
	}

	resizeGeneralToolbarButtons();
}

void MainWindow::setPluginsActions(ModelWidget *model_wgt)
{
	if(!model_wgt)
		return;

	PluginsConfigWidget *plugins_conf_wgt = dynamic_cast<PluginsConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::PluginsConfWgt));
	model_wgt->setPluginActions(plugins_conf_wgt->getPluginsModelsActions());
}

void MainWindow::createMainWidgets()
{
	try
	{
		layers_cfg_wgt = new LayersConfigWidget(this);
		layers_cfg_wgt->setVisible(false);

		changelog_wgt  = new ChangelogWidget(this);
		changelog_wgt->setVisible(false);

		scene_info_wgt = new SceneInfoWidget(this);
		scene_info_wgt->setObjectName("scene_info_wgt");
		QHBoxLayout *hbox = new QHBoxLayout(scene_info_parent);
		hbox->addWidget(scene_info_wgt);
		hbox->setContentsMargins(GuiUtilsNs::LtMargin,GuiUtilsNs::LtMargin,GuiUtilsNs::LtMargin,GuiUtilsNs::LtMargin);
		scene_info_parent->setLayout(hbox);

		welcome_wgt=new WelcomeWidget(views_stw);
		welcome_wgt->setObjectName("welcome_wgt");
		QGridLayout *grid=new QGridLayout;
		grid->setContentsMargins(0,0,0,0);
		grid->setSpacing(0);
		grid->addWidget(welcome_wgt, 0, 0);
		views_stw->widget(WelcomeView)->setLayout(grid);

		sql_tool_wgt=new SQLToolWidget;
		sql_tool_wgt->setObjectName("sql_tool_wgt");
		grid=new QGridLayout;
		grid->setContentsMargins(0,0,0,0);
		grid->setSpacing(0);
		grid->addWidget(sql_tool_wgt, 0, 0);
		views_stw->widget(ManageView)->setLayout(grid);

		model_nav_wgt=new ModelNavigationWidget(this);
		model_nav_wgt->setObjectName("model_nav_wgt");

		about_wgt=new AboutWidget(this);
		donate_wgt=new DonateWidget(this);
		restoration_form=new ModelRestorationForm(nullptr, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

	#ifdef NO_UPDATE_CHECK
		update_notifier_wgt=nullptr;
	#else
		update_notifier_wgt=new UpdateNotifierWidget(this);
		update_notifier_wgt->setVisible(false);
	#endif

		oper_list_wgt=new OperationListWidget;
		model_objs_wgt=new ModelObjectsWidget;
		overview_wgt=new ModelOverviewWidget;
		model_valid_wgt=new ModelValidationWidget;
		obj_finder_wgt=new ObjectFinderWidget;
	}
	catch(Exception &e)
	{
		Messagebox msgbox;

		/* In case of initialization problems related to broken configuration files
		 * We try to restore them so the next initialization can occur without problems */
		msgbox.show(e, tr("Failed to initialize one or more components of the UI due to corrupted or incompatible configuration files. Running the CLI tool to restore the default settings may solve this issue. How do you want to proceed?"),
								Messagebox::ErrorIcon, Messagebox::YesNoButtons,
								tr("Restore"), tr("Abort"), "",
								GuiUtilsNs::getIconPath("defaults"), GuiUtilsNs::getIconPath("cancel"), "");

		// Running the CLI in config file restoration mode is the user accepts the message box
		if(msgbox.result() == QDialog::Accepted)
		{
			QProcess proc;
			proc.setProgram(GlobalAttributes::getPgModelerCLIPath());
			proc.setArguments({ "-cc", "-ff", "--silent" });
			proc.start();
			proc.waitForFinished();

			if(proc.error() != QProcess::UnknownError || proc.exitCode() != 0)
			{
				msgbox.show(tr("The CLI failed to restore the configuration files! \
 The command executed was: <br/><br/> <strong>%1</strong> \
 <br/><br/> Error(s) returned: <br/><br/><em>%2</em>")
											 .arg(proc.program() + " " + proc.arguments().join(" "),
														proc.readAllStandardOutput()), Messagebox::ErrorIcon);
			}
			else
			{
				msgbox.show(tr("The default settings were successfully restored! pgModeler will be restarted now so the configuration files can be correctly loaded."),
										Messagebox::InfoIcon);

				// Starting a new instance of pgModeler (detached)
				proc.setProgram(GlobalAttributes::getPgModelerAppPath());
				proc.setArguments({});
				proc.startDetached();
				proc.waitForFinished();
			}
		}

		throw Exception(e.getErrorMessage(), e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}

void MainWindow::loadConfigurations()
{
	try
	{
		configuration_form=new ConfigurationForm(nullptr, Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
		GuiUtilsNs::resizeDialog(configuration_form);
		configuration_form->loadConfiguration();

		PluginsConfigWidget *plugins_conf_wgt = dynamic_cast<PluginsConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::PluginsConfWgt));
		plugins_conf_wgt->initPlugins(this);
		plugins_conf_wgt->installPluginsActions(plugins_config_menu, plugins_tb_acts, plugins_db_expl_btns);
		plugins_config_menu->setEnabled(!plugins_config_menu->isEmpty());
		sql_tool_wgt->setPluginsToolButtons(plugins_db_expl_btns);

		QAction *action_plugins_config = plugins_config_menu->menuAction();
		action_plugins_config->setText(tr("Plug-ins"));
		action_plugins_config->setToolTip(tr("Access the loaded plug-ins settings"));
		action_plugins_config->setEnabled(!plugins_config_menu->isEmpty());

		//Configuring the widget visibility according to the configurations
		for(auto &itr : GeneralConfigWidget::getConfigurationParams())
		{
			if(itr.second.count(Attributes::Path)!=0)
			{
				try
				{
					//Storing the file of a previous session
					if(itr.first.contains(Attributes::File) &&
							!itr.second[Attributes::Path].isEmpty())
						prev_session_files.push_back(itr.second[Attributes::Path]);

					//Creating the recent models menu
					else if(itr.first.contains(Attributes::Recent) &&
							!itr.second[Attributes::Path].isEmpty())
						recent_models.push_back(itr.second[Attributes::Path]);
				}
				catch(Exception &e)
				{
					Messagebox msg_box;
					msg_box.show(e);
				}
			}
		}
	}
	catch(Exception &e)
	{
		Messagebox msg_box;
		msg_box.show(e);
	}
}

void MainWindow::connectSignalsToSlots()
{
	connect(welcome_wgt->new_tb, &QToolButton::clicked, this, [this]() {
		addModel();
	});

	connect(welcome_wgt->load_tb, &QToolButton::clicked, this, qOverload<>(&MainWindow::loadModel));
	connect(welcome_wgt->last_session_tb, &QToolButton::clicked, this, &MainWindow::restoreLastSession);
	connect(welcome_wgt->support_tb, &QToolButton::clicked, this, &MainWindow::openSupport);

#ifndef NO_UPDATE_CHECK
	connect(update_notifier_wgt, &UpdateNotifierWidget::s_updateAvailable, action_update_found, &QAction::setVisible, Qt::QueuedConnection);
	connect(update_notifier_wgt, &UpdateNotifierWidget::s_updateAvailable, action_update_found, &QAction::setChecked, Qt::QueuedConnection);
	connect(update_notifier_wgt, &UpdateNotifierWidget::s_hideRequested, action_update_found, &QAction::toggle, Qt::QueuedConnection);
	connect(action_update_found, &QAction::toggled, this, &MainWindow::toggleUpdateNotifier, Qt::QueuedConnection);
	connect(action_check_update, &QAction::triggered, update_notifier_wgt, &UpdateNotifierWidget::checkForUpdate);
#endif

	connect(action_about, &QAction::toggled, this, &MainWindow::toggleAboutWidget);
	connect(about_wgt, &AboutWidget::s_hideRequested, action_about, &QAction::toggle);

	connect(action_donate, &QAction::toggled, this, &MainWindow::toggleDonateWidget);
	connect(donate_wgt, &DonateWidget::s_hideRequested, action_donate, &QAction::toggle);

	connect(action_restore_session, &QAction::triggered, this, &MainWindow::restoreLastSession);
	connect(action_exit, &QAction::triggered, this, &MainWindow::close);

	connect(action_new_model, &QAction::triggered, this, [this]() {
		addModel();
	});

	connect(action_close_model, &QAction::triggered, this, &MainWindow::closeModel);

	connect(action_fix_model, &QAction::triggered, this, [this]() {
		fixModel();
	});

	connect(action_support, &QAction::triggered, this, &MainWindow::openSupport);

	connect(action_inc_zoom, &QAction::triggered, this, &MainWindow::applyZoom);
	connect(action_dec_zoom, &QAction::triggered, this, &MainWindow::applyZoom);
	connect(action_normal_zoom, &QAction::triggered, this, &MainWindow::applyZoom);

	connect(action_load_model, &QAction::triggered, this, qOverload<>(&MainWindow::loadModel));

	connect(action_save_model, &QAction::triggered, this, [this]() {
		saveModel();
	});

	connect(action_save_as, &QAction::triggered, this, [this]() {
		saveModel();
	});

	connect(action_save_all, &QAction::triggered, this, &MainWindow::saveAllModels);
	connect(oper_list_wgt, &OperationListWidget::s_operationExecuted, this, &MainWindow::updateDockWidgets);
	connect(oper_list_wgt, &OperationListWidget::s_operationListUpdated, this, &MainWindow::__updateToolsState);
	connect(action_undo, &QAction::triggered, oper_list_wgt, &OperationListWidget::undoOperation);
	connect(action_redo, &QAction::triggered, oper_list_wgt, &OperationListWidget::redoOperation);

	connect(model_nav_wgt, &ModelNavigationWidget::s_modelCloseRequested, this, &MainWindow::closeModel);
	connect(model_nav_wgt, &ModelNavigationWidget::s_currentModelChanged, this, &MainWindow::setCurrentModel);

	connect(action_print, &QAction::triggered, this, &MainWindow::printModel);

	connect(action_configuration, &QAction::triggered, this, [this](){
		GeneralConfigWidget::restoreWidgetGeometry(configuration_form);
		configuration_form->exec();
		GeneralConfigWidget::saveWidgetGeometry(configuration_form);
	});

	connect(oper_list_wgt, &OperationListWidget::s_operationExecuted, overview_wgt, qOverload<>(&ModelOverviewWidget::updateOverview));
	connect(oper_list_wgt, &OperationListWidget::s_operationExecuted, layers_cfg_wgt, &LayersConfigWidget::updateLayersRects);
	connect(layers_cfg_wgt, &LayersConfigWidget::s_activeLayersChanged, overview_wgt, qOverload<>(&ModelOverviewWidget::updateOverview));

	connect(configuration_form, &ConfigurationForm::finished, this, &MainWindow::applyConfigurations);

	connect(configuration_form, &ConfigurationForm::rejected, this, [this]() {
		updateConnections();
	});

	connect(configuration_form, &ConfigurationForm::s_invalidateModelsRequested, this, [this](){
		//Forcing the models code invalidation if the user changes the escape comments option
		for(int idx = 0; idx < models_tbw->count(); idx++)
			dynamic_cast<ModelWidget *>(models_tbw->widget(idx))->getDatabaseModel()->setCodesInvalidated();
	});

	connect(&model_save_timer, &QTimer::timeout, this, &MainWindow::saveAllModels);

	connect(action_export, &QAction::triggered, this, &MainWindow::exportModel);
	connect(action_import, &QAction::triggered, this, &MainWindow::importDatabase);
	connect(action_diff, &QAction::triggered, this, &MainWindow::diffModelDatabase);

	connect(action_welcome, &QAction::triggered, this, &MainWindow::changeCurrentView);
	connect(action_design, &QAction::triggered, this, &MainWindow::changeCurrentView);
	connect(action_manage, &QAction::triggered, this, &MainWindow::changeCurrentView);
	connect(action_manage, &QAction::toggled, this, &MainWindow::changeCurrentView);

	connect(action_bug_report, &QAction::triggered, this, &MainWindow::reportBug);
	connect(action_handle_metadata, &QAction::triggered, this, &MainWindow::handleObjectsMetadata);

	connect(model_valid_wgt, &ModelValidationWidget::s_connectionsUpdateRequest, this, [this](){
		updateConnections(true);
	});

	connect(sql_tool_wgt, &SQLToolWidget::s_connectionsUpdateRequest, this, [this](){
		updateConnections(true);
	});

	connect(action_compact_view, &QAction::triggered, this, &MainWindow::toggleCompactView);

	connect(objects_btn, &QToolButton::toggled, model_objs_parent, &QWidget::setVisible);
	connect(objects_btn, &QToolButton::toggled, model_objs_wgt, &ModelObjectsWidget::setVisible);
	connect(objects_btn, &QToolButton::toggled, this, &MainWindow::showRightWidgetsBar);
	connect(model_objs_wgt, qOverload<bool>(&ModelObjectsWidget::s_visibilityChanged), objects_btn, &QToolButton::setChecked);
	connect(model_objs_wgt, qOverload<bool>(&ModelObjectsWidget::s_visibilityChanged), this, &MainWindow::showRightWidgetsBar);

	connect(operations_btn, &QToolButton::toggled, oper_list_parent, &QWidget::setVisible);
	connect(operations_btn, &QToolButton::toggled, oper_list_wgt, &OperationListWidget::setVisible);
	connect(operations_btn, &QToolButton::toggled, this, &MainWindow::showRightWidgetsBar);
	connect(oper_list_wgt, &OperationListWidget::s_hideRequested, operations_btn, &QToolButton::toggle);
	connect(oper_list_wgt, &OperationListWidget::s_hideRequested, this, &MainWindow::showRightWidgetsBar);

	connect(validation_btn, &QToolButton::toggled, model_valid_parent, &QWidget::setVisible);
	connect(validation_btn, &QToolButton::toggled, model_valid_wgt, &ModelValidationWidget::setVisible);
	connect(validation_btn, &QToolButton::toggled, this, &MainWindow::showBottomWidgetsBar);
	connect(model_valid_wgt, &ModelValidationWidget::s_hideRequested, validation_btn, &QToolButton::toggle);
	connect(model_valid_wgt, &ModelValidationWidget::s_hideRequested, this, &MainWindow::showBottomWidgetsBar);

	connect(find_obj_btn, &QToolButton::toggled, obj_finder_parent, &QWidget::setVisible);
	connect(find_obj_btn, &QToolButton::toggled, obj_finder_wgt, &ObjectFinderWidget::setVisible);
	connect(find_obj_btn, &QToolButton::toggled, this, &MainWindow::showBottomWidgetsBar);
	connect(obj_finder_wgt, &ObjectFinderWidget::s_hideRequested, find_obj_btn, &QToolButton::toggle);
	connect(obj_finder_wgt, &ObjectFinderWidget::s_hideRequested, this, &MainWindow::showBottomWidgetsBar);

	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, this->main_menu_mb, &QMenu::setDisabled);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, model_acts_tb, &QToolBar::setDisabled);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, tools_acts_tb, &QToolBar::setDisabled);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, models_tbw, &QTabWidget::setDisabled);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, oper_list_wgt, &OperationListWidget::setDisabled);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, model_objs_wgt, &ModelObjectsWidget::setDisabled);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, obj_finder_wgt, &ObjectFinderWidget::setDisabled);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, models_tbw, &QTabWidget::setDisabled);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, this, &MainWindow::stopTimers);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, layers_btn, &QToolButton::setDisabled);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, layers_cfg_wgt, &LayersConfigWidget::close);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, changelog_btn, &QToolButton::setDisabled);
	connect(model_valid_wgt, &ModelValidationWidget::s_validationInProgress, changelog_wgt, &ChangelogWidget::close);

	connect(model_valid_wgt, &ModelValidationWidget::s_validationCanceled, this, [this](){
		pending_op=NoPendingOp;
	});

	connect(model_valid_wgt, &ModelValidationWidget::s_validationFinished, this, &MainWindow::executePendingOperation);
	connect(model_valid_wgt, &ModelValidationWidget::s_fixApplied, this, &MainWindow::removeOperations, Qt::QueuedConnection);
	connect(model_valid_wgt, &ModelValidationWidget::s_graphicalObjectsUpdated, model_objs_wgt, &ModelObjectsWidget::updateObjectsView, Qt::QueuedConnection);

	connect(layers_btn, &QToolButton::toggled, this, &MainWindow::toggleLayersWidget);
	connect(layers_cfg_wgt, &LayersConfigWidget::s_visibilityChanged, layers_btn, &QToolButton::setChecked);

	connect(changelog_btn, &QToolButton::toggled, this, &MainWindow::toggleChangelogWidget);
	connect(changelog_wgt, &ChangelogWidget::s_visibilityChanged, changelog_btn, &QToolButton::setChecked);

	connect(&tmpmodel_save_timer, &QTimer::timeout, this, &MainWindow::saveTemporaryModels);

#ifndef Q_OS_MAC
	connect(action_show_main_menu, &QAction::triggered, this, &MainWindow::showMainMenu);
	connect(action_hide_main_menu, &QAction::triggered, this, &MainWindow::showMainMenu);
#endif
}

void MainWindow::restoreTemporaryModels()
{
	QStringList ignored_files;

	/* If we have loaded models prior the execution of the restoration form
	 * (e.g, the user has started pgModeler from a double click on a .dbm file)
	 * we just need to ignore their temp files so the restoration form shows just files
	 * created on a previous session */
	for(int idx = 0; idx < models_tbw->count(); idx++)
		ignored_files += QFileInfo(dynamic_cast<ModelWidget *>(models_tbw->widget(idx))->getTempFilename()).fileName();

	GuiUtilsNs::resizeDialog(restoration_form);
	restoration_form->setIgnoredFiles(ignored_files);

	//Restore temporary models (if exists)
	if(restoration_form->hasTemporaryModels())
	{
		restoration_form->exec();

		if(restoration_form->result()==QDialog::Accepted)
		{
			ModelWidget *model=nullptr;
			QString model_file;
			QStringList tmp_models=restoration_form->getSelectedModels();

			while(!tmp_models.isEmpty())
			{
				try
				{
					model_file=tmp_models.front();
					tmp_models.pop_front();
					this->addModel(model_file);

					//Get the model widget generated from file
					model=dynamic_cast<ModelWidget *>(models_tbw->widget(models_tbw->count()-1));

					//Set the model as modified forcing the user to save when the autosave timer ends
					model->setModified(true);
					model->filename.clear();
					restoration_form->removeTemporaryModel(model_file);
				}
				catch(Exception &e)
				{
					//Destroy the temp file if the "keep  models" isn't checked
					if(!restoration_form->keep_models_chk->isChecked())
						restoration_form->removeTemporaryModel(model_file);

					Messagebox msg_box;
					msg_box.show(e);
				}
			}
		}
	}
}

bool MainWindow::isToolButtonsChecked(QHBoxLayout *layout, const QWidgetList &ignored_wgts)
{
	int i = 0;
	bool show = false;
	QToolButton * btn = nullptr;

	//This is currently the only way to enumerate widgets inside a layout.
	//See https://stackoverflow.com/a/27225570/7359123
	while(layout && layout->itemAt(i) && !show)
	{
		btn = dynamic_cast<QToolButton *>(layout->itemAt(i)->widget());
		i++;

		if(ignored_wgts.contains(btn))
			continue;

		if(btn && btn->isChecked())
			return true;
	}

	return false;
}

void MainWindow::resizeGeneralToolbarButtons()
{
	QToolButton *btn = nullptr;

	if(tools_acts_tb->minimumWidth() == 0)
		tools_acts_tb->setMinimumWidth(tools_acts_tb->width() *
																(BaseObjectView::getScreenDpiFactor() < BaseObjectView::MaxDpiFactor ? 0.60 : 0.90));

	for(auto &act : tools_acts_tb->actions())
	{
		btn = qobject_cast<QToolButton *>(tools_acts_tb->widgetForAction(act));
		if(!btn) continue;

		btn->setStyleSheet(QString("QToolButton { min-width: %1px; margin-top: 2px; }")
											 .arg(models_tbw->count() == 0 ? tools_acts_tb->minimumWidth() : tools_acts_tb->minimumWidth() * 1.10));
	}
}

void MainWindow::showRightWidgetsBar()
{
	right_wgt_bar->setVisible(isToolButtonsChecked(vert_wgts_btns_layout));
}

void MainWindow::showBottomWidgetsBar()
{
	bottom_wgt_bar->setVisible(isToolButtonsChecked(horiz_wgts_btns_layout, {layers_btn, changelog_btn}));
}

void MainWindow::restoreLastSession()
{
	/* Loading the files from the previous session. The session will be restored only
	if pgModeler is not on model restore mode or pgModeler is not opening a model clicked by user
	o the file manager */
	if(QApplication::arguments().size() <= 1 &&
			!prev_session_files.isEmpty() && restoration_form->result()==QDialog::Rejected)
	{
		try
		{
			qApp->setOverrideCursor(Qt::WaitCursor);

			while(!prev_session_files.isEmpty())
			{
				this->addModel(prev_session_files.front());
				prev_session_files.pop_front();
			}

			action_restore_session->setEnabled(false);
			welcome_wgt->last_session_tb->setEnabled(false);
			qApp->restoreOverrideCursor();
		}
		catch(Exception &e)
		{
			qApp->restoreOverrideCursor();
			Messagebox msg_box;
			msg_box.show(e);
		}
	}
}

void MainWindow::stopTimers(bool value)
{
	if(value)
	{
		tmpmodel_save_timer.stop();
		model_save_timer.stop();
	}
	else
	{
		tmpmodel_save_timer.start();

		if(model_save_timer.interval() < InfinityInterval)
			model_save_timer.start();
	}
}

void MainWindow::fixModel(const QString &filename)
{
	ModelFixForm model_fix_form(nullptr, Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

	connect(&model_fix_form, &ModelFixForm::s_modelLoadRequested,
					this, qOverload<const QString &>(&MainWindow::loadModel),
					Qt::QueuedConnection);

	if(!filename.isEmpty())
	{
		QFileInfo fi(filename);
		model_fix_form.input_file_sel->setSelectedFile(fi.absoluteFilePath());
		model_fix_form.output_file_sel->setSelectedFile(fi.absolutePath() + GlobalAttributes::DirSeparator + fi.baseName() + "_fixed." + fi.suffix());
	}

	GuiUtilsNs::resizeDialog(&model_fix_form);
	GeneralConfigWidget::restoreWidgetGeometry(&model_fix_form);
	model_fix_form.exec();
	GeneralConfigWidget::saveWidgetGeometry(&model_fix_form);

	disconnect(&model_fix_form, nullptr, this, nullptr);
}

void MainWindow::resizeEvent(QResizeEvent *)
{
	if(welcome_wgt)
	{
		welcome_wgt->move(bg_widget->width()/2 - welcome_wgt->width()/2 ,
							bg_widget->height()/2 - welcome_wgt->height()/2);
	}

	action_about->setChecked(false);
	action_donate->setChecked(false);
	action_update_found->setChecked(false);

	toggleLayersWidget(layers_cfg_wgt->isVisible());
	toggleChangelogWidget(changelog_wgt->isVisible());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	//pgModeler will not close when the validation thread is still running
	if(model_valid_wgt->isValidationRunning())
		event->ignore();
	else
	{
		GeneralConfigWidget *conf_wgt=nullptr;
		std::map<QString, attribs_map > confs;

		GeneralConfigWidget::saveWidgetGeometry(this);

		//Stops the saving timers as well the temp. model saving thread before close pgmodeler
		model_save_timer.stop();
		tmpmodel_save_timer.stop();
		plugins_config_menu->clear();

		//If not in demo version there is no confirmation before close the software
#ifndef DEMO_VERSION
		int i=0;
		QStringList model_names;
		ModelWidget *model=nullptr;

		//Checking if there is modified models and ask the user to save them before close the application
		if(models_tbw->count() > 0)
		{
			i=0;
			while(i < models_tbw->count())
			{
				model=dynamic_cast<ModelWidget *>(models_tbw->widget(i++));

				if(model->isModified())
					model_names.push_back(QString("<strong>%1</strong>").arg(model->getDatabaseModel()->getName()));
			}

			if(!model_names.isEmpty())
			{
				Messagebox msg_box;

				msg_box.show(tr("Save modified model(s)"),
							 tr("The following models were modified but not saved: %1. Do you really want to quit pgModeler?").arg(model_names.join(", ")),
							 Messagebox::ConfirmIcon,Messagebox::YesNoButtons);

				/* If the user rejects the message box the close event will be aborted
				causing pgModeler not to be finished */
				if(msg_box.result()==QDialog::Rejected)
					event->ignore();
			}
		}
#else
		showDemoVersionWarning(true);
#endif

		if(event->isAccepted())
		{
			int i, count;
			ModelWidget *model=nullptr;
			QString param_id;
			attribs_map attribs;

			this->overview_wgt->close();
			conf_wgt=dynamic_cast<GeneralConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::GeneralConfWgt));
			confs=conf_wgt->getConfigurationParams();

			attribs[Attributes::PgModelerVersion]=GlobalAttributes::PgModelerVersion;
			attribs[Attributes::FirstRun]=Attributes::False;

			attribs[Attributes::CompactView]=action_compact_view->isChecked() ? Attributes::True : "";
			attribs[Attributes::ShowMainMenu]=main_menu_mb->isVisible() ? Attributes::True : "";

			conf_wgt->addConfigurationParam(Attributes::Configuration, attribs);
			attribs.clear();

			count=models_tbw->count();

			//Remove the references to old session
			conf_wgt->removeConfigurationParam(QRegularExpression(QString("(%1)([0-9])+").arg(Attributes::File)));

			//Saving the session
			for(i=0; i < count; i++)
			{
				model=dynamic_cast<ModelWidget *>(models_tbw->widget(i));

				if(!model->getFilename().isEmpty() &&
					 /* Models loaded from temporary dir are not included in the session
						* since they are removed once pgModeler is closed */
					 !model->getFilename().contains(GlobalAttributes::getTemporaryPath()))
				{
					param_id=QString("%1%2").arg(Attributes::File).arg(i);
					attribs[Attributes::Id]=param_id;
					attribs[Attributes::Path]=model->getFilename();
					conf_wgt->addConfigurationParam(param_id, attribs);
					attribs.clear();
				}
			}

			//Saving recent models list
			if(!recent_models.isEmpty())
			{
				int i=0;
				QString param_id;
				attribs_map attribs;

				while(!recent_models.isEmpty())
				{
					param_id=QString("%1%2").arg(Attributes::Recent).arg(QString::number(i++).rightJustified(2, '0'));
					attribs[Attributes::Id]=param_id;
					attribs[Attributes::Path]=recent_models.front();
					conf_wgt->addConfigurationParam(param_id, attribs);
					attribs.clear();
					recent_models.pop_front();
				}

				recent_models_menu->clear();
			}
			else
				conf_wgt->removeConfigurationParam(QRegularExpression(QString("(%1)(.)+").arg(Attributes::Recent)));

			//Saving dock widgets settings
			storeDockWidgetsSettings();

			conf_wgt->saveConfiguration();
			restoration_form->removeTemporaryFiles();

			SQLExecutionWidget::saveSQLHistory();
			qApp->quit();
		}
	}
}

void MainWindow::updateConnections(bool force)
{
	ConnectionsConfigWidget *conn_cfg_wgt=
			dynamic_cast<ConnectionsConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::ConnectionsConfWgt));

	if(force || (!force && (conn_cfg_wgt->isConfigurationChanged() ||
							model_valid_wgt->connections_cmb->count()==0 ||
							sql_tool_wgt->connections_cmb->count()==0)))
	{
		if(sender()!=sql_tool_wgt)
		{
			ConnectionsConfigWidget::fillConnectionsComboBox(sql_tool_wgt->connections_cmb, true);
			sql_tool_wgt->clearDatabases();
		}

		if(sender()!=model_valid_wgt)
			ConnectionsConfigWidget::fillConnectionsComboBox(model_valid_wgt->connections_cmb, true, Connection::OpValidation);
	}
}

void MainWindow::saveTemporaryModels()
{
#ifdef DEMO_VERSION
#warning "DEMO VERSION: temporary model saving disabled."
#else
	try
	{
		ModelWidget *model=nullptr;
		int count=models_tbw->count();

		if(count > 0)
		{
			qApp->setOverrideCursor(Qt::WaitCursor);
			scene_info_parent->setVisible(false);
			bg_saving_wgt->setVisible(true);
			bg_saving_pb->setValue(0);
			bg_saving_wgt->repaint();

			for(int i=0; i < count; i++)
			{
				model=dynamic_cast<ModelWidget *>(models_tbw->widget(i));
				bg_saving_pb->setValue(((i+1)/static_cast<double>(count)) * 100);

				if(model->isModified())
					model->getDatabaseModel()->saveModel(model->getTempFilename(), SchemaParser::XmlCode);
			}

			bg_saving_pb->setValue(100);
			bg_saving_wgt->setVisible(false);
			scene_info_parent->setVisible(true);
			qApp->restoreOverrideCursor();
		}

		tmpmodel_save_timer.start();
	}
	catch(Exception &e)
	{
		qApp->restoreOverrideCursor();
		Messagebox msg_box;
		msg_box.show(e);
		tmpmodel_save_timer.start();
	}
#endif
}

void MainWindow::updateRecentModelsMenu()
{
	QAction *act=nullptr;
	QFileInfo fi;

	recent_models_menu->setToolTipsVisible(true);
	recent_models_menu->clear();
	recent_models.removeDuplicates();

	for(int i=0; i < recent_models.size() && i < GeneralConfigWidget::MaxRecentModels; i++)
	{
		act=recent_models_menu->addAction(QFileInfo(recent_models[i]).fileName(),this, &MainWindow::loadModelFromAction);
		act->setToolTip(recent_models[i]);
		act->setData(recent_models[i]);

		if(recent_models[i].endsWith(GlobalAttributes::DbModelExt))
			act->setIcon(QIcon(GuiUtilsNs::getIconPath("dbmfile")));
		else
		{
			fi.setFile(recent_models[i]);
			if(recent_models_icons.contains(fi.suffix()))
				act->setIcon(recent_models_icons[fi.suffix()]);
		}
	}

	if(!recent_models_menu->isEmpty())
	{
		recent_models_menu->addSeparator();
		recent_models_menu->addAction(QIcon(GuiUtilsNs::getIconPath("delete")), tr("Clear menu"), this, &MainWindow::clearRecentModelsMenu);
	}

	recent_models_menu->menuAction()->setEnabled(!recent_models_menu->isEmpty());
	welcome_wgt->recent_tb->setEnabled(recent_models_menu->menuAction()->isEnabled());
	welcome_wgt->recent_tb->setMenu(recent_models_menu->isEmpty() ? nullptr : recent_models_menu);
}

void MainWindow::loadModelFromAction()
{
	QAction *act=dynamic_cast<QAction *>(sender());

	if(act)
	{
		QString filename=act->data().toString();

		try
		{
			qApp->setOverrideCursor(Qt::WaitCursor);
			addModel(filename);
			registerRecentModel(filename);
			qApp->restoreOverrideCursor();
		}
		catch(Exception &e)
		{
			qApp->restoreOverrideCursor();
			if(QFileInfo(filename).exists())
				showFixMessage(e, filename);
			else
				throw Exception(e.getErrorMessage(),e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__,&e);
		}
	}
}

void MainWindow::clearRecentModelsMenu()
{
	recent_models.clear();
	updateRecentModelsMenu();
}

void MainWindow::addModel(const QString &filename)
{
#ifdef DEMO_VERSION
#warning "DEMO VERSION: database model creation limit."
	if(models_tbw->count()==1)
		throw Exception(tr("The demonstration version can create only `one' instance of database model!"),
										ErrorCode::Custom,__PRETTY_FUNCTION__,__FILE__,__LINE__);
#endif

	try
	{
		ModelWidget *model_tab=nullptr;
		QString obj_name, tab_name, str_aux;
		Schema *public_sch=nullptr;
		bool start_timers=(models_tbw->count() == 0);

		//Set a name for the tab widget
		str_aux=QString("%1").arg(models_tbw->count());
		obj_name="model_";
		obj_name+=str_aux;
		tab_name=obj_name;

		model_tab=new ModelWidget;
		model_tab->setObjectName(obj_name);
		setPluginsActions(model_tab);

		//Add the tab to the tab widget
		obj_name=model_tab->db_model->getName();

		models_tbw->blockSignals(true);
		models_tbw->setUpdatesEnabled(false);
		models_tbw->addTab(model_tab, obj_name);
		models_tbw->setCurrentIndex(models_tbw->count()-1);
		models_tbw->blockSignals(false);
		models_tbw->currentWidget()->layout()->setContentsMargins(0,0,0,0);

		//Creating the system objects (public schema and languages C, SQL and pgpgsql)
		model_tab->db_model->createSystemObjects(filename.isEmpty());

		if(filename.isEmpty())
			model_tab->updateSceneLayers();
		else
		{
			try
			{
				model_tab->loadModel(filename);
				models_tbw->setTabToolTip(models_tbw->currentIndex(), filename);

				//Get the "public" schema and set as system object
				public_sch=dynamic_cast<Schema *>(model_tab->db_model->getObject("public", ObjectType::Schema));
				if(public_sch)	public_sch->setSystemObject(true);

				model_tab->db_model->setInvalidated(false);
				model_tab->restoreLastCanvasPosition();

				//Making a copy of the loaded database model file as the first version of the temp. model
				QFile::copy(filename, model_tab->getTempFilename());
			}
			catch(Exception &e)
			{
				models_tbw->setUpdatesEnabled(true);
				welcome_wgt->update();
				models_tbw->removeTab(models_tbw->indexOf(model_tab));
				model_tab->setParent(nullptr);

				//Destroy the temp file generated by allocating a new model widget
				restoration_form->removeTemporaryModel(model_tab->getTempFilename());

				updateToolsState(true);

				throw Exception(e.getErrorMessage(),e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
			}
		}

		model_nav_wgt->addModel(model_tab);
		models_tbw->setUpdatesEnabled(true);
		models_tbw->setVisible(true);
		setCurrentModel();

		if(start_timers)
		{
			if(model_save_timer.interval() > 0)
				model_save_timer.start();

			tmpmodel_save_timer.start();
		}

		model_tab->setModified(false);
		action_save_model->setEnabled(false);

		if(action_alin_objs_grade->isChecked())
			current_model->scene->alignObjectsToGrid();
	}
	catch(Exception &e)
	{
		throw Exception(e.getErrorMessage(),e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}

void MainWindow::addModel(ModelWidget *model_wgt)
{
	try
	{
		if(!model_wgt)
			throw Exception(ErrorCode::AsgNotAllocattedObject,__PRETTY_FUNCTION__,__FILE__,__LINE__);
		else if(model_wgt->parent())
			throw Exception(ErrorCode::AsgWidgetAlreadyHasParent,__PRETTY_FUNCTION__,__FILE__,__LINE__);

		setPluginsActions(model_wgt);

		model_nav_wgt->addModel(model_wgt);
		models_tbw->blockSignals(true);
		models_tbw->addTab(model_wgt, model_wgt->getDatabaseModel()->getName());
		models_tbw->setCurrentIndex(models_tbw->count()-1);
		models_tbw->blockSignals(false);

		setCurrentModel();
		models_tbw->currentWidget()->layout()->setContentsMargins(0,0,0,0);

		if(action_alin_objs_grade->isChecked())
			current_model->scene->alignObjectsToGrid();
	}
	catch(Exception &e)
	{
		throw Exception(e.getErrorMessage(),e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}

int MainWindow::getModelCount()
{
	return models_tbw->count();
}

ModelWidget *MainWindow::getModel(int idx)
{
	if(idx < 0 || idx > models_tbw->count())
		throw Exception(ErrorCode::RefObjectInvalidIndex,__PRETTY_FUNCTION__,__FILE__,__LINE__);

	return dynamic_cast<ModelWidget *>(models_tbw->widget(idx));
}

void MainWindow::showMainMenu()
{
	main_menu.menuAction()->setVisible(sender()!=action_show_main_menu);
	main_menu_mb->setVisible(sender()==action_show_main_menu);

	if(sender()!=action_show_main_menu)
		file_menu->removeAction(action_hide_main_menu);
	else
		file_menu->addAction(action_hide_main_menu);
}

void MainWindow::setCurrentModel()
{
	layers_cfg_wgt->setVisible(false);
	models_tbw->setVisible(models_tbw->count() > 0);

	action_design->setEnabled(models_tbw->count() > 0);
	action_magnifier->setChecked(false);

	if(models_tbw->count() > 0)
		action_design->activate(QAction::Trigger);
	else
		action_welcome->activate(QAction::Trigger);

	removeModelActions();

	edit_menu->clear();
	edit_menu->addAction(action_undo);
	edit_menu->addAction(action_redo);
	edit_menu->addSeparator();

	//Avoids the tree state saving in order to restore the current model tree state
	model_objs_wgt->saveTreeState(false);

	//Restore the tree state
	if(current_model)
		model_objs_wgt->saveTreeState(model_tree_states[current_model]);

	models_tbw->setCurrentIndex(model_nav_wgt->getCurrentIndex());
	current_model=dynamic_cast<ModelWidget *>(models_tbw->currentWidget());
	arrange_menu.menuAction()->setEnabled(current_model != nullptr);

	QFile::remove(GlobalAttributes::getTemporaryFilePath(GlobalAttributes::LastModelFile));

	if(current_model)
	{
		/* Each time the current model is changed we have to change the last model link so the
		 * so the crash handler and bug report form can indentify the last modified model after
		 * their execution */
		#ifdef Q_OS_WIN
			// On Windows, since there's no symbolic links like Linux/macOs we forcibly create a copy of the current model as the last model
			QFile::copy(current_model->getTempFilename(), GlobalAttributes::getTemporaryFilePath(GlobalAttributes::LastModelFile));
		#else
			QFile::link(current_model->getTempFilename(), GlobalAttributes::getTemporaryFilePath(GlobalAttributes::LastModelFile));
		#endif

		QToolButton *tool_btn=nullptr;
		QList<QToolButton *> btns;
		QFont font;

		layers_cfg_wgt->setModel(current_model);
		current_model->setFocus(Qt::OtherFocusReason);
		current_model->cancelObjectAddition();
		tools_acts_tb->addSeparator();

		tools_acts_tb->addAction(current_model->action_new_object);
		tool_btn=qobject_cast<QToolButton *>(tools_acts_tb->widgetForAction(current_model->action_new_object));
		tool_btn->setPopupMode(QToolButton::InstantPopup);
		btns.push_back(tool_btn);

		tools_acts_tb->addAction(current_model->action_quick_actions);
		tool_btn=qobject_cast<QToolButton *>(tools_acts_tb->widgetForAction(current_model->action_quick_actions));
		tool_btn->setPopupMode(QToolButton::InstantPopup);
		btns.push_back(tool_btn);

		tools_acts_tb->addAction(current_model->action_edit);
		tool_btn=qobject_cast<QToolButton *>(tools_acts_tb->widgetForAction(current_model->action_edit));
		btns.push_back(tool_btn);

		tools_acts_tb->addAction(current_model->action_source_code);
		tool_btn=qobject_cast<QToolButton *>(tools_acts_tb->widgetForAction(current_model->action_source_code));
		btns.push_back(tool_btn);

		configureMoreActionsMenu();
		tools_acts_tb->addAction(more_actions_menu.menuAction());

		tool_btn = qobject_cast<QToolButton *>(tools_acts_tb->widgetForAction(more_actions_menu.menuAction()));
		tool_btn->setPopupMode(QToolButton::InstantPopup);
		btns.push_back(tool_btn);

		for(QToolButton *btn : btns)
		{
			font = btn->font();
			font.setWeight(QFont::Normal);
			btn->setFont(font);
			GuiUtilsNs::createDropShadow(btn, 1, 1, 5);
		}

		edit_menu->addAction(current_model->action_copy);
		edit_menu->addAction(current_model->action_cut);
		edit_menu->addAction(current_model->action_duplicate);
		edit_menu->addAction(current_model->action_paste);
		edit_menu->addAction(current_model->action_remove);
		edit_menu->addAction(current_model->action_cascade_del);

		connect(current_model, &ModelWidget::s_modelModified, model_nav_wgt, &ModelNavigationWidget::setCurrentModelModified, Qt::UniqueConnection);

		connect(current_model, &ModelWidget::s_modelModified, this, [this](bool modified) {
			if(modified) updateToolsState();
		});

		connect(current_model, &ModelWidget::s_manipulationCanceled, oper_list_wgt, &OperationListWidget::updateOperationList, Qt::UniqueConnection);
		connect(current_model, &ModelWidget::s_objectsMoved, oper_list_wgt, &OperationListWidget::updateOperationList, Qt::UniqueConnection);
		connect(current_model, &ModelWidget::s_objectModified,this, &MainWindow::updateDockWidgets, Qt::UniqueConnection);
		connect(current_model, &ModelWidget::s_objectCreated,this, &MainWindow::updateDockWidgets, Qt::UniqueConnection);
		connect(current_model, &ModelWidget::s_objectRemoved,this, &MainWindow::updateDockWidgets, Qt::UniqueConnection);
		connect(current_model, &ModelWidget::s_objectManipulated,this, &MainWindow::updateDockWidgets, Qt::UniqueConnection);
		connect(current_model, &ModelWidget::s_objectManipulated, this, &MainWindow::updateModelTabName, Qt::UniqueConnection);
		connect(current_model, &ModelWidget::s_zoomModified, this, &MainWindow::__updateToolsState, Qt::UniqueConnection);
		connect(current_model, &ModelWidget::s_objectModified, this, &MainWindow::updateModelTabName, Qt::UniqueConnection);

		connect(current_model, qOverload<BaseObjectView *>(&ModelWidget::s_sceneInteracted), this, &MainWindow::configureMoreActionsMenu, Qt::UniqueConnection);
		connect(current_model, qOverload<BaseObjectView *>(&ModelWidget::s_sceneInteracted), scene_info_wgt, &SceneInfoWidget::updateSelectedObject, Qt::UniqueConnection);
		connect(current_model, qOverload<int, const QRectF &>(&ModelWidget::s_sceneInteracted), scene_info_wgt, &SceneInfoWidget::updateSelectedObjects, Qt::UniqueConnection);
		connect(current_model, qOverload<const QPointF &>(&ModelWidget::s_sceneInteracted), scene_info_wgt, &SceneInfoWidget::updateMousePosition, Qt::UniqueConnection);
		connect(current_model, &ModelWidget::s_zoomModified, scene_info_wgt, &SceneInfoWidget::updateSceneZoom, Qt::UniqueConnection);

		connect(current_model, &ModelWidget::s_zoomModified, this, [this](double zoom) {
			ObjectsScene::setLockDelimiterScale(action_lock_delim->isChecked(), zoom);
			current_model->update();
		});

		connect(action_alin_objs_grade, &QAction::triggered, this, &MainWindow::setGridOptions, Qt::UniqueConnection);
		connect(action_show_grid, &QAction::triggered, this, &MainWindow::setGridOptions, Qt::UniqueConnection);
		connect(action_show_delimiters, &QAction::triggered, this, &MainWindow::setGridOptions, Qt::UniqueConnection);
		connect(action_lock_delim, &QAction::triggered, this, &MainWindow::setGridOptions, Qt::UniqueConnection);

		connect(action_magnifier, &QAction::toggled, current_model, &ModelWidget::showMagnifierArea, Qt::UniqueConnection);
		connect(action_overview, &QAction::toggled, this, &MainWindow::showOverview, Qt::UniqueConnection);
		connect(overview_wgt, &ModelOverviewWidget::s_overviewVisible, action_overview, &QAction::setChecked, Qt::UniqueConnection);\

		connect(current_model, &ModelWidget::s_maginifierAreaVisible, this, [this](bool show) {
			action_magnifier->blockSignals(true);
			action_magnifier->setChecked(show);
			action_magnifier->blockSignals(false);
		});

		if(action_overview->isChecked())
			overview_wgt->show(current_model);

		scene_info_wgt->updateMousePosition(QPointF(0,0));
		scene_info_wgt->updateSceneZoom(current_model->getCurrentZoom());
		current_model->emitSceneInteracted();

		ObjectsScene::setLockDelimiterScale(action_lock_delim->isChecked(),
																				current_model->getCurrentZoom());
		current_model->update();
	}

	updateWindowTitle();

	edit_menu->addSeparator();
	edit_menu->addAction(action_configuration);

	updateToolsState();

	oper_list_wgt->setModel(current_model);
	model_objs_wgt->setModel(current_model);
	model_valid_wgt->setModel(current_model);
	obj_finder_wgt->setModel(current_model);
	changelog_wgt->setModel(current_model);

	if(current_model)
		model_objs_wgt->restoreTreeState(model_tree_states[current_model]);

	model_objs_wgt->saveTreeState(true);

	resizeGeneralToolbarButtons();

	emit s_currentModelChanged(current_model);
}

void MainWindow::setGridOptions()
{
	GeneralConfigWidget *conf_wgt = dynamic_cast<GeneralConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::GeneralConfWgt));
	std::map<QString, attribs_map> attribs = conf_wgt->getConfigurationParams();

	ObjectsScene::setShowGrid(action_show_grid->isChecked());
	ObjectsScene::setShowPageDelimiters(action_show_delimiters->isChecked());
	ObjectsScene::setAlignObjectsToGrid(action_alin_objs_grade->isChecked());
	ObjectsScene::setLockDelimiterScale(action_lock_delim->isChecked(),
																			current_model ? current_model->getCurrentZoom() : 1);

	if(current_model)
	{
		//Align the object to grid is the option is checked
		if(action_alin_objs_grade->isChecked())
		{
			current_model->scene->alignObjectsToGrid();

			//Forcing the relationship updating to fit the new position of the tables
			current_model->getDatabaseModel()->setObjectsModified({ ObjectType::Relationship, ObjectType::BaseRelationship });
		}

		//Redraw the scene to apply the new grid options
		current_model->update();
	}

	conf_wgt->addConfigurationParam(Attributes::Configuration, attribs[Attributes::Configuration]);
}

void MainWindow::applyZoom()
{
	if(current_model)
	{
		double zoom = current_model->getCurrentZoom();

		if(sender() == action_normal_zoom)
			zoom = 1;
		else if(sender() == action_inc_zoom && zoom < ModelWidget::MaximumZoom)
			zoom += ModelWidget::ZoomIncrement;
		else if(sender() == action_dec_zoom && zoom > ModelWidget::MinimumZoom)
			zoom -= ModelWidget::ZoomIncrement;

		current_model->applyZoom(zoom);
		updateToolsState();
	}
}

void MainWindow::removeModelActions()
{
	QList<QAction *> act_list;
	act_list=tools_acts_tb->actions();

	while(act_list.size() > ToolsActionsCount)
	{
		tools_acts_tb->removeAction(act_list.back());
		act_list.pop_back();
	}
}

void MainWindow::closeModel(int model_id)
{
	QWidget *tab=nullptr;

	overview_wgt->close();

	if(model_id >= 0)
		tab=models_tbw->widget(model_id);
	else
		tab=models_tbw->currentWidget();

	if(tab)
	{
		ModelWidget *model=dynamic_cast<ModelWidget *>(tab);
		Messagebox msg_box;

#ifdef DEMO_VERSION
		msg_box.setResult(QDialog::Accepted);
#else
		//Ask the user to save the model if its modified
		if(model->isModified())
		{
			msg_box.show(tr("Save model"),
						 tr("The model <strong>%1</strong> was modified! Do you really want to close without saving it?").arg(model->getDatabaseModel()->getName()),
						 Messagebox::ConfirmIcon, Messagebox::YesNoButtons);
		}
#endif

		if(!model->isModified() ||
				(model->isModified() && msg_box.result()==QDialog::Accepted))
		{
			model_nav_wgt->removeModel(model_id);
			model_tree_states.erase(model);

			disconnect(model, nullptr, nullptr, nullptr);

			//Remove the temporary file related to the closed model
			QDir arq_tmp;
			arq_tmp.remove(model->getTempFilename());

			//Removing model specific actions from general toolbar
			removeModelActions();

			if(model_id >= 0)
				models_tbw->removeTab(model_id);
			else
				models_tbw->removeTab(models_tbw->currentIndex());
		}
	}

	if(models_tbw->count()==0)
	{
		current_model=nullptr;
		setCurrentModel();
		model_save_timer.stop();
		tmpmodel_save_timer.stop();
		models_tbw->setVisible(false);
	}
	else
	{
		setCurrentModel();
	}
}

void MainWindow::updateModelTabName()
{
	if(current_model && current_model->db_model->getName() != models_tbw->tabText(models_tbw->currentIndex()))
		model_nav_wgt->updateModelText(models_tbw->currentIndex(), current_model->db_model->getName(), current_model->getFilename());
}

void MainWindow::updateWindowTitle()
{
	if(!current_model || current_model->getFilename().isEmpty())
		setWindowTitle(window_title);
	else
		setWindowTitle(window_title + " - " + QDir::toNativeSeparators(current_model->getFilename()));
}

void MainWindow::applyConfigurations()
{
  if(!sender() ||
			(sender()==configuration_form && configuration_form->result()==QDialog::Accepted))
	{
		GeneralConfigWidget *conf_wgt=nullptr;
		int count, i;
		ModelWidget *model=nullptr;

		conf_wgt=dynamic_cast<GeneralConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::GeneralConfWgt));

		//Disable the auto save if the option is not checked
		if(!conf_wgt->autosave_interv_chk->isChecked())
		{
			//Stop the save timer
			model_save_timer.setInterval(InfinityInterval);
			model_save_timer.stop();
		}
		else
		{
			model_save_timer.setInterval(conf_wgt->autosave_interv_spb->value() * 60000);
			model_save_timer.start();
		}

		//Temporary models are saved every five minutes
		tmpmodel_save_timer.setInterval(model_save_timer.interval() < InfinityInterval ? model_save_timer.interval()/2 : 300000);
		tmpmodel_save_timer.start();

		qApp->setOverrideCursor(Qt::WaitCursor);

		//Force the update of all opened models
		count=models_tbw->count();
		for(i=0; i < count; i++)
		{
			model=dynamic_cast<ModelWidget *>(models_tbw->widget(i));
			model->updateObjectsOpacity();
			model->db_model->setObjectsModified();
		}

		if(current_model)
			current_model->update();

		updateConnections();
		sql_tool_wgt->configureSnippets();
		sql_tool_wgt->reloadHighlightConfigs();

		qApp->restoreOverrideCursor();
	}

	sql_tool_wgt->updateTabs();
}


void MainWindow::saveAllModels()
{
	if(models_tbw->count() > 0 &&
			((sender()==action_save_all) ||
			 (sender()==&model_save_timer &&	this->isActiveWindow())))

	{
		int i, count;

		count=models_tbw->count();
		for(i=0; i < count; i++)
			this->saveModel(dynamic_cast<ModelWidget *>(models_tbw->widget(i)));
	}
}

void MainWindow::saveModel(ModelWidget *model)
{
#ifdef DEMO_VERSION
#warning "DEMO VERSION: model saving disabled."
	Messagebox msg_box;
	msg_box.show(tr("Warning"),
				 tr("You're running a demonstration version! The model saving feature is available only in the full version!"),
				 Messagebox::AlertIcon, Messagebox::OkButton);
#else
	try
	{
		if(!model)
			model = current_model;

		if(model)
		{
			Messagebox msg_box;
			DatabaseModel *db_model=model->getDatabaseModel();

			if(confirm_validation && db_model->isInvalidated())
			{
				msg_box.show(tr("Confirmation"),
							 tr(" <strong>WARNING:</strong> The model <strong>%1</strong> is invalidated! It's recommended to validate it before save in order to create a consistent model otherwise the generated file will be broken demanding manual fixes to be loadable again!").arg(db_model->getName()),
							 Messagebox::AlertIcon, Messagebox::AllButtons,
							 tr("Validate"), tr("Save anyway"), "",
								GuiUtilsNs::getIconPath("validation"), GuiUtilsNs::getIconPath("save"));

				//If the user cancel the saving force the stopping of autosave timer to give user the chance to validate the model
				if(msg_box.isCancelled())
				{
					model_save_timer.stop();

					//The autosave timer will be reactivated in 5 minutes
					QTimer::singleShot(300000, &model_save_timer, qOverload<>(&QTimer::start));
				}
				else if(msg_box.result()==QDialog::Accepted)
				{
					validation_btn->setChecked(true);
					this->pending_op=(sender()==action_save_as ? PendingSaveAsOp : PendingSaveOp);
					action_design->setChecked(true);
					model_valid_wgt->validateModel();
				}
			}

			stopTimers(true);

			if((!confirm_validation ||
					(!db_model->isInvalidated() ||
					 (confirm_validation && db_model->isInvalidated() && !msg_box.isCancelled() && msg_box.result()==QDialog::Rejected)))
				 && (model->isModified() || sender()==action_save_as))
			{
				//If the action that calls the slot were the 'save as' or the model filename isn't set
				if(sender()==action_save_as || model->filename.isEmpty() || pending_op==PendingSaveAsOp)
				{
					QStringList sel_files = GuiUtilsNs::selectFiles(
																		tr("Save '%1' as...").arg(model->db_model->getName()),
																		QFileDialog::AnyFile,	QFileDialog::AcceptSave,
																		{ tr("Database model (*%1)").arg(GlobalAttributes::DbModelExt),
																			tr("All files (*.*)") }, {},
																		GlobalAttributes::DbModelExt);

					if(!sel_files.isEmpty())
					{
						model->saveModel(sel_files.at(0));
						registerRecentModel(sel_files.at(0));
						model_nav_wgt->updateModelText(models_tbw->indexOf(model), model->getDatabaseModel()->getName(), sel_files.at(0));
					}
				}
				else
				{
					bool save_model = true;
					ModelWidget *aux_model = nullptr;

					/* We check if the model being save is loaded in other tab(s). If so, we raise a warning message related to the
					 * risk of overwriting and possible data/work loss if the model being saved is an older version */
					for(int idx = 0; idx < models_tbw->count(); idx++)
					{
						aux_model = dynamic_cast<ModelWidget *>(models_tbw->widget(idx));

						if(model != aux_model && model->getFilename() == aux_model->getFilename())
						{
							msg_box.show(tr("<strong>WARNING:</strong> the database model <strong>%1</strong>, file <strong>%2</strong>, is also loaded in another tab! Saving the current model to the file may lead to data loss if its version in memory is outdated compared to what is loaded in the other tab. Do you really want to proceed with the saving?")
													 .arg(model->getDatabaseModel()->getName()).arg(model->getFilename()),
													 Messagebox::AlertIcon, Messagebox::YesNoButtons);

							save_model = msg_box.result() == QDialog::Accepted;
							break;
						}
					}

					if(save_model)
						model->saveModel();
				}

				updateWindowTitle();
				model_valid_wgt->clearOutput();
				emit s_modelSaved(model);
			}

			stopTimers(false);
			action_save_model->setEnabled(model->isModified());
		}
	}
	catch(Exception &e)
	{
		stopTimers(false);
		throw Exception(e.getErrorMessage(),e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
#endif
}

void MainWindow::exportModel()
{
	ModelExportForm model_export_form(nullptr, Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	Messagebox msg_box;
	DatabaseModel *db_model=current_model->getDatabaseModel();

	action_design->setChecked(true);

	if(confirm_validation && db_model->isInvalidated())
	{
		msg_box.show(tr("Confirmation"),
					 tr(" <strong>WARNING:</strong> The model <strong>%1</strong> is invalidated! Before run the export process it's recommended to validate in order to correctly create the objects on database server!").arg(db_model->getName()),
					 Messagebox::AlertIcon, Messagebox::AllButtons,
					 tr("Validate"), tr("Export anyway"), "",
					 GuiUtilsNs::getIconPath("validation"), GuiUtilsNs::getIconPath("export"));

		if(msg_box.result()==QDialog::Accepted)
		{
			validation_btn->setChecked(true);
			this->pending_op=PendingExportOp;
			model_valid_wgt->validateModel();
		}
	}

	if(!confirm_validation ||
			(!db_model->isInvalidated() || (confirm_validation && !msg_box.isCancelled() && msg_box.result()==QDialog::Rejected)))
	{
		stopTimers(true);
		connect(&model_export_form, &ModelExportForm::s_connectionsUpdateRequest, this, [this](){ updateConnections(true); });

		GuiUtilsNs::resizeDialog(&model_export_form);
		GeneralConfigWidget::restoreWidgetGeometry(&model_export_form);
		model_export_form.exec(current_model);
		GeneralConfigWidget::saveWidgetGeometry(&model_export_form);

		stopTimers(false);
	}
}

void MainWindow::importDatabase()
{
	DatabaseImportForm db_import_form(nullptr, Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

	stopTimers(true);

	connect(&db_import_form, &DatabaseImportForm::s_connectionsUpdateRequest, this, [this](){ updateConnections(true); });

	db_import_form.setModelWidget(current_model);
	GuiUtilsNs::resizeDialog(&db_import_form);

	GeneralConfigWidget::restoreWidgetGeometry(&db_import_form);
	db_import_form.exec();
	GeneralConfigWidget::saveWidgetGeometry(&db_import_form);

	stopTimers(false);

	if(db_import_form.result()==QDialog::Accepted && db_import_form.getModelWidget())
		this->addModel(db_import_form.getModelWidget());
	else if(current_model)
		updateDockWidgets();
}

void MainWindow::diffModelDatabase()
{
	ModelDatabaseDiffForm modeldb_diff_frm(nullptr, Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	Messagebox msg_box;
	DatabaseModel *db_model=(current_model ? current_model->getDatabaseModel() : nullptr);

	if(current_model)
		action_design->setChecked(true);

	if(confirm_validation && db_model && db_model->isInvalidated())
	{
		msg_box.show(tr("Confirmation"),
					 tr(" <strong>WARNING:</strong> The model <strong>%1</strong> is invalidated! Before run the diff process it's recommended to validate in order to correctly analyze and generate the difference between the model and a database!").arg(db_model->getName()),
					 Messagebox::AlertIcon, Messagebox::AllButtons,
					 tr("Validate"), tr("Diff anyway"), "",
					 GuiUtilsNs::getIconPath("validation"), GuiUtilsNs::getIconPath("diff"));

		if(msg_box.result()==QDialog::Accepted)
		{
			validation_btn->setChecked(true);
			this->pending_op=PendingDiffOp;
			model_valid_wgt->validateModel();
		}
	}

	if(!confirm_validation || !db_model ||
			((db_model && !db_model->isInvalidated()) || (confirm_validation && !msg_box.isCancelled() && msg_box.result()==QDialog::Rejected)))
	{
		modeldb_diff_frm.setModelWidget(current_model);

		stopTimers(true);
		connect(&modeldb_diff_frm, &ModelDatabaseDiffForm::s_connectionsUpdateRequest, this, [this](){ updateConnections(true); });
		connect(&modeldb_diff_frm, &ModelDatabaseDiffForm::s_loadDiffInSQLTool, this, [this](QString conn_id, QString database, QString filename){
			action_manage->setChecked(true);
			sql_tool_wgt->addSQLExecutionTab(conn_id, database, filename);
		});

		GeneralConfigWidget::restoreWidgetGeometry(&modeldb_diff_frm);
		modeldb_diff_frm.exec();
		GeneralConfigWidget::saveWidgetGeometry(&modeldb_diff_frm);
		stopTimers(false);
	}
}

void MainWindow::printModel()
{
	if(current_model)
	{
		QPrintDialog print_dlg;
		QPrinter *printer=nullptr;
		QPageLayout curr_page_lt, orig_page_lt;
		GeneralConfigWidget *conf_wgt=dynamic_cast<GeneralConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::GeneralConfWgt));

		print_dlg.setOption(QAbstractPrintDialog::PrintCurrentPage, false);
		print_dlg.setWindowTitle(tr("Database model printing"));

		//Get the current page configuration of the scene
		orig_page_lt = ObjectsScene::getPageLayout();

		//Get a reference to the printer
		printer = print_dlg.printer();

		//Sets the printer options based upon the configurations from the scene
		printer->setPageLayout(orig_page_lt);
		print_dlg.exec();

		//If the user confirms the printing
		if(print_dlg.result() == QDialog::Accepted)
		{
			Messagebox msg_box;

			// Checking If the user modified the default settings on the printer overriding the scene configurations
			curr_page_lt = printer->pageLayout();

			if(orig_page_lt.marginsPoints() != curr_page_lt.marginsPoints() ||
				 orig_page_lt.pageSize().id() != curr_page_lt.pageSize().id() ||
				 orig_page_lt.pageSize().rectPoints() != curr_page_lt.pageSize().rectPoints())
			{
				msg_box.show("", tr("Changes were detected in the definitions of paper/margin which may cause the incorrect printing of the objects. Which configurations do you want to use?"),
										 Messagebox::AlertIcon, Messagebox::AllButtons,
										 tr("New settings"), tr("Defaults"), tr("Cancel"),
										 GuiUtilsNs::getIconPath("new"),
										 GuiUtilsNs::getIconPath("defaults"));
			}

			if(!msg_box.isCancelled())
			{
				if(msg_box.result()==QDialog::Rejected)
				{
					// Retore the page configuration to the scene's default
					printer->setPageLayout(orig_page_lt);
				}

				current_model->printModel(printer,
																	conf_wgt->print_grid_chk->isChecked(),
																	conf_wgt->print_pg_num_chk->isChecked(),
																	true);
			}
		}
	}
}

void MainWindow::loadModel()
{
	try
	{
		QStringList sel_files = GuiUtilsNs::selectFiles(
															tr("Load model"),
															QFileDialog::ExistingFiles,	QFileDialog::AcceptOpen,
															{ tr("Database model (*%1)").arg(GlobalAttributes::DbModelExt),
																tr("All files (*.*)") }, {},
															GlobalAttributes::DbModelExt);

		if(!sel_files.isEmpty())
			loadModels(sel_files);
	}
	catch(Exception &e)
	{
		Messagebox msg_box;
		msg_box.show(e);
	}
}

void MainWindow::loadModel(const QString &filename)
{
	loadModels({ filename });
}

void MainWindow::loadModels(const QStringList &files)
{
	if(files.isEmpty())
		return;

	int i = -1;

	try
	{
		qApp->setOverrideCursor(Qt::WaitCursor);

		for(auto &file : files)
		{
			i++;

			if(!file.endsWith(GlobalAttributes::DbModelExt))
			{
				emit s_modelLoadRequested(file);
				continue;
			}

			addModel(file);
			recent_models.push_front(file);
		}

		updateRecentModelsMenu();
		qApp->restoreOverrideCursor();
	}
	catch(Exception &e)
	{
		qApp->restoreOverrideCursor();

		if( files[i].endsWith(GlobalAttributes::DbModelExt))
			showFixMessage(e, files[i]);
		else
			throw Exception(e.getErrorMessage(), e.getErrorCode(), __PRETTY_FUNCTION__, __FILE__, __LINE__, &e);
	}
}

void MainWindow::showFixMessage(Exception &e, const QString &filename)
{
	Messagebox msg_box;

	msg_box.show(Exception(Exception::getErrorMessage(ErrorCode::ModelFileNotLoaded).arg(filename),
												 ErrorCode::ModelFileNotLoaded ,__PRETTY_FUNCTION__,__FILE__,__LINE__, &e),
							 tr("Could not load the database model file `%1'. Check the error stack to see details. You can try to fix it in order to make it loadable again.").arg(filename),
							 Messagebox::ErrorIcon, Messagebox::YesNoButtons,
							 tr("Fix model"), tr("Cancel"), "",
							 GuiUtilsNs::getIconPath("fixobject"), GuiUtilsNs::getIconPath("cancel"));

	if(msg_box.result()==QDialog::Accepted)
		fixModel(filename);
}

void MainWindow::setConfirmValidation(bool value)
{
	confirm_validation=value;
}

ModelWidget *MainWindow::getCurrentModel()
{
	return current_model;
}

void MainWindow::__updateToolsState()
{
	updateToolsState(false);
}

void MainWindow::updateToolsState(bool model_closed)
{
	bool enabled=(!model_closed && current_model);

	action_print->setEnabled(enabled);
	action_save_as->setEnabled(enabled);
	action_save_model->setEnabled(enabled && current_model->isModified());
	action_save_all->setEnabled(enabled);
	action_export->setEnabled(enabled);
	action_close_model->setEnabled(enabled);
	action_show_grid->setEnabled(enabled);
	action_show_delimiters->setEnabled(enabled);
	action_lock_delim->setEnabled(enabled);
	action_overview->setEnabled(enabled);

	action_normal_zoom->setEnabled(enabled);
	action_inc_zoom->setEnabled(enabled && current_model->getCurrentZoom() < ModelWidget::MaximumZoom);
	action_dec_zoom->setEnabled(enabled && current_model->getCurrentZoom() > ModelWidget::MinimumZoom);
	action_normal_zoom->setEnabled(enabled && (current_model->getCurrentZoom() * 100) != 100);
	action_alin_objs_grade->setEnabled(enabled);
	action_undo->setEnabled(enabled);
	action_redo->setEnabled(enabled);
	action_compact_view->setEnabled(enabled);
	action_magnifier->setEnabled(enabled && current_model->getCurrentZoom() < 1);

	action_handle_metadata->setEnabled(enabled);

	if(!model_closed && current_model && models_tbw->count() > 0)
	{
		action_undo->setEnabled(current_model->op_list->isUndoAvailable());
		action_redo->setEnabled(current_model->op_list->isRedoAvailable());
	}
}

void MainWindow::updateDockWidgets()
{
	oper_list_wgt->updateOperationList();
	model_objs_wgt->updateObjectsView();

	/* Any operation executed over the model will reset the validation and
	the finder will execute the search again */
	model_valid_wgt->setModel(current_model);

	if(current_model && obj_finder_wgt->result_tbw->rowCount() > 0)
	  obj_finder_wgt->findObjects();
}

void MainWindow::showOverview(bool show)
{
	if(show && current_model && !overview_wgt->isVisible())
		overview_wgt->show(current_model);
	else if(!show)
		overview_wgt->close();
}

void MainWindow::openSupport()
{
	QDesktopServices::openUrl(QUrl(GlobalAttributes::PgModelerSupport));
}

void MainWindow::toggleUpdateNotifier(bool show)
{
#ifndef NO_UPDATE_CHECK
	if(show)
	{
		setFloatingWidgetPos(update_notifier_wgt,  qobject_cast<QAction *>(sender()), model_acts_tb, false);
		action_about->setChecked(false);
		action_donate->setChecked(false);
	}

	update_notifier_wgt->adjustSize();
	update_notifier_wgt->setVisible(show);
#endif
}

void MainWindow::toggleAboutWidget(bool show)
{
	if(show)
	{
		setFloatingWidgetPos(about_wgt, qobject_cast<QAction *>(sender()), model_acts_tb, false);
		action_update_found->setChecked(false);
		action_donate->setChecked(false);
	}

	about_wgt->adjustSize();
	about_wgt->setVisible(show);
}

void MainWindow::toggleDonateWidget(bool show)
{
	if(show)
	{
		setFloatingWidgetPos(donate_wgt, qobject_cast<QAction *>(sender()), model_acts_tb, false);
		action_about->setChecked(false);
		action_update_found->setChecked(false);
	}

	donate_wgt->adjustSize();
	donate_wgt->setVisible(show);
}

void MainWindow::setFloatingWidgetPos(QWidget *widget, QAction *act, QToolBar *toolbar, bool map_to_window)
{
	if(!widget || !act  || !toolbar)
		return;

	QWidget *wgt=toolbar->widgetForAction(act);
	QPoint pos_orig=(wgt ? wgt->pos() : QPoint(0,0)), pos;

	if(map_to_window)
		pos=wgt->mapTo(this, pos);

	pos.setX(pos_orig.x() - 10);
	pos.setY(toolbar->pos().y() + toolbar->height() /* - 10 */);

	if((pos.x() + widget->width()) > this->width())
		pos.setX(pos_orig.x() - (widget->width() - 40));

	widget->move(pos);
}

void MainWindow::setBottomFloatingWidgetPos(QWidget *widget, QToolButton *btn)
{
	if(!widget || !btn)
		return;

	QPoint btn_parent_pos = mapTo(this, tool_btns_bar_wgt->pos()),
			btn_pos = mapTo(this, btn->pos());

	int py = btn_parent_pos.y() - (widget->height() - btn->height() - (2 * v_splitter1->handleWidth())) + 1;

	if(main_menu_mb->isVisible())
		py += main_menu_mb->height();

	widget->move(btn_pos.x() + tools_acts_tb->width() + 1, py);
}

void MainWindow::configureSamplesMenu()
{
	QDir dir(GlobalAttributes::getSamplesPath());
	QStringList files=dir.entryList({ QString("*%1").arg(GlobalAttributes::DbModelExt) });
	QAction *act=nullptr;
	QString path;

	while(!files.isEmpty())
	{
		act=sample_mdls_menu.addAction(files.front(), this, &MainWindow::loadModelFromAction);
		path=QFileInfo(GlobalAttributes::getSamplesPath() + GlobalAttributes::DirSeparator + files.front()).absoluteFilePath();
		act->setToolTip(path);
		act->setData(path);
		act->setIcon(QIcon(GuiUtilsNs::getIconPath("dbmfile")));
		files.pop_front();
	}

	if(sample_mdls_menu.isEmpty())
	{
		act=sample_mdls_menu.addAction(tr("(no samples found)"));
		act->setEnabled(false);
	}

	sample_mdls_menu.setToolTipsVisible(true);
	welcome_wgt->sample_tb->setMenu(&sample_mdls_menu);
}

void MainWindow::storeDockWidgetsSettings()
{
	GeneralConfigWidget *conf_wgt=dynamic_cast<GeneralConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::GeneralConfWgt));
	attribs_map params;

	params[Attributes::Validator]=Attributes::True;
	params[Attributes::SqlValidation]=(model_valid_wgt->sql_validation_chk->isChecked() ? Attributes::True : "");
	params[Attributes::UseUniqueNames]=(model_valid_wgt->use_tmp_names_chk->isChecked() ? Attributes::True : "");
	params[Attributes::Version]=model_valid_wgt->version_cmb->currentText();
	conf_wgt->addConfigurationParam(Attributes::Validator, params);
	params.clear();

	params[Attributes::ObjectFinder]=Attributes::True;
	params[Attributes::SelectObjects]=(obj_finder_wgt->select_btn->isChecked() ? Attributes::True : "");
	params[Attributes::FadeInObjects]=(obj_finder_wgt->fade_btn->isChecked() ? Attributes::True : "");
	params[Attributes::RegularExp]=(obj_finder_wgt->regexp_chk->isChecked() ? Attributes::True : "");
	params[Attributes::CaseSensitive]=(obj_finder_wgt->case_sensitive_chk->isChecked() ? Attributes::True : "");
	params[Attributes::ExactMatch]=(obj_finder_wgt->exact_match_chk->isChecked() ? Attributes::True : "");
	conf_wgt->addConfigurationParam(Attributes::ObjectFinder, params);
	params.clear();

	params[Attributes::SqlTool]=Attributes::True;
	params[Attributes::ShowAttributesGrid]=(sql_tool_wgt->attributes_tb->isChecked() ? Attributes::True : "");
	params[Attributes::ShowSourcePane]=(sql_tool_wgt->source_pane_tb->isChecked() ? Attributes::True : "");
	conf_wgt->addConfigurationParam(Attributes::SqlTool, params);
	params.clear();
}

void MainWindow::restoreDockWidgetsSettings()
{
	GeneralConfigWidget *conf_wgt=dynamic_cast<GeneralConfigWidget *>(configuration_form->getConfigurationWidget(ConfigurationForm::GeneralConfWgt));
	std::map<QString, attribs_map> confs=conf_wgt->getConfigurationParams();

	if(confs.count(Attributes::Validator))
	{
		model_valid_wgt->sql_validation_chk->setChecked(confs[Attributes::Validator][Attributes::SqlValidation]==Attributes::True);
		model_valid_wgt->use_tmp_names_chk->setChecked(confs[Attributes::Validator][Attributes::UseUniqueNames]==Attributes::True);
		model_valid_wgt->version_cmb->setCurrentText(confs[Attributes::Validator][Attributes::Version]);
	}

	if(confs.count(Attributes::ObjectFinder))
	{
		obj_finder_wgt->select_btn->setChecked(confs[Attributes::ObjectFinder][Attributes::SelectObjects]==Attributes::True);
		obj_finder_wgt->fade_btn->setChecked(confs[Attributes::ObjectFinder][Attributes::FadeInObjects]==Attributes::True);
		obj_finder_wgt->regexp_chk->setChecked(confs[Attributes::ObjectFinder][Attributes::RegularExp]==Attributes::True);
		obj_finder_wgt->case_sensitive_chk->setChecked(confs[Attributes::ObjectFinder][Attributes::CaseSensitive]==Attributes::True);
		obj_finder_wgt->exact_match_chk->setChecked(confs[Attributes::ObjectFinder][Attributes::ExactMatch]==Attributes::True);
	}

	if(confs.count(Attributes::SqlTool))
	{
		sql_tool_wgt->attributes_tb->setChecked(confs[Attributes::SqlTool][Attributes::ShowAttributesGrid]==Attributes::True);
		sql_tool_wgt->source_pane_tb->setChecked(confs[Attributes::SqlTool][Attributes::ShowSourcePane]==Attributes::True);
	}
}

#ifdef DEMO_VERSION
void MainWindow::showDemoVersionWarning(bool exit_msg)
{
	Messagebox msg_box;

	if(!exit_msg)
	{
		msg_box.show(tr("Warning"),
					 tr("You're running a demonstration version of pgModeler! Note that you are able to create only <strong>%1</strong> instances \
							of each type of object and some key features like <strong>saving the model</strong>, <strong>code generation</strong>, and some others will be disabled or limited!<br/><br/>Please, support this project <a href='%2'>buying a full binary copy</a>, use the promo code <strong>DEMOTESTER</strong> and receive a special discount on any purchase. You can also get the <a href='%3'>source code</a> and compile it yourself.\
							<strong>NOTE:</strong> pgModeler is open-source software, but purchasing binary copies or providing a donation of any amount will support the project and keep its development alive!<br/><br/>\
							<strong>HINT:</strong> in order to test all features it's recommended to use the <strong>demo.dbm</strong> model located in </strong>Sample models</strong> at <strong>Welcome</strong> view.")
							.arg(GlobalAttributes::MaxObjectCount).arg(GlobalAttributes::PgModelerDownloadURL + "?purchase=true&promocode=DEMOTESTER", GlobalAttributes::PgModelerDownloadURL + "?source=true"),
							Messagebox::AlertIcon, Messagebox::OkButton);
	}
	else
	{
		msg_box.show(tr("Info"),
					 tr("Thank you for testing pgModeler! Don't forget that you can support this project by <a href='%2'>buying a full binary copy</a> using the promo code <strong>DEMOTESTER</strong> to receive a special discount. Also, you can get the <a href='%3'>source code</a> and compile it yourself, instructions on the site.")
							.arg(GlobalAttributes::PgModelerDownloadURL + "?purchase=true&promocode=DEMOTESTER", GlobalAttributes::PgModelerDownloadURL + "?source=true"),
							Messagebox::InfoIcon, Messagebox::OkButton);
	}
}
#endif

void MainWindow::executePendingOperation(bool valid_error)
{
	if(!valid_error && pending_op!=NoPendingOp)
	{
		static const QString op_names[]={ "", QT_TR_NOOP("save"), QT_TR_NOOP("save"),
																			QT_TR_NOOP("export"), QT_TR_NOOP("diff") },

		op_icons[]={ "", GuiUtilsNs::getIconPath("save"), GuiUtilsNs::getIconPath("saveas"),
								 GuiUtilsNs::getIconPath("export"), GuiUtilsNs::getIconPath("diff") };

		GuiUtilsNs::createOutputTreeItem(model_valid_wgt->output_trw,
																		 tr("Executing pending <strong>%1</strong> operation...").arg(op_names[pending_op]),
																		 op_icons[pending_op]);

		if(pending_op==PendingSaveOp || pending_op==PendingSaveAsOp)
			saveModel();
		else if(pending_op==PendingExportOp)
			exportModel();
		else if(pending_op==PendingDiffOp)
			diffModelDatabase();

		pending_op=NoPendingOp;
	}
}

void MainWindow::changeCurrentView(bool checked)
{
	QAction *curr_act=qobject_cast<QAction *>(sender());

	layers_cfg_wgt->setVisible(false);
	changelog_wgt->setVisible(false);

	if(checked)
	{
		bool enable=(curr_act==action_design);
		QList<QAction *> actions;

		action_welcome->blockSignals(true);
		action_manage->blockSignals(true);
		action_design->blockSignals(true);

		action_welcome->setChecked(false);
		action_manage->setChecked(false);
		action_design->setChecked(false);

		curr_act->setChecked(true);
		views_stw->setCurrentIndex(curr_act->data().toInt());

		action_welcome->blockSignals(false);
		action_manage->blockSignals(false);
		action_design->blockSignals(false);

		actions=tools_acts_tb->actions();
		for(int i=ToolsActionsCount; i < actions.count(); i++)
			actions[i]->setEnabled(enable);

		if(!enable)
			overview_wgt->close();

		actions=edit_menu->actions();
		actions.removeOne(action_configuration);
		for(auto &act : actions)
			act->setEnabled(enable);

		actions=show_menu->actions();
		for(auto &act : actions)
			act->setEnabled(enable);

		model_nav_wgt->setEnabled(enable);
		action_print->setEnabled(enable);
		action_close_model->setEnabled(enable);
		action_save_as->setEnabled(enable);
	}
	else
	{
		curr_act->blockSignals(true);
		curr_act->setChecked(true);
		curr_act->blockSignals(false);
	}
}

void MainWindow::reportBug()
{
	BugReportForm bugrep_frm;
	GuiUtilsNs::resizeDialog(&bugrep_frm);
	GeneralConfigWidget::restoreWidgetGeometry(&bugrep_frm);
	bugrep_frm.exec();
	GeneralConfigWidget::saveWidgetGeometry(&bugrep_frm);
}

void MainWindow::removeOperations()
{
	//Clears the operation list everytime a fix is applied to the model
	if(current_model && current_model->op_list->getCurrentSize()!=0)
	{
		current_model->op_list->removeOperations();
		oper_list_wgt->updateOperationList();
	}
}

void MainWindow::handleObjectsMetadata()
{
	MetadataHandlingForm objs_meta_frm(nullptr, Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

	objs_meta_frm.setModelWidget(current_model);
	objs_meta_frm.setModelWidgets(model_nav_wgt->getModelWidgets());

	connect(&objs_meta_frm, &MetadataHandlingForm::s_metadataHandled, model_objs_wgt, &ModelObjectsWidget::updateObjectsView);
	connect(&objs_meta_frm, &MetadataHandlingForm::s_metadataHandled, layers_cfg_wgt, &LayersConfigWidget::updateLayersList);

	GuiUtilsNs::resizeDialog(&objs_meta_frm);
	GeneralConfigWidget::restoreWidgetGeometry(&objs_meta_frm);
	objs_meta_frm.exec();
	GeneralConfigWidget::saveWidgetGeometry(&objs_meta_frm);
}

void MainWindow::arrangeObjects()
{
	if(!current_model)
		return;

	Messagebox msgbox;
	msgbox.show(tr("Rearrange objects over the canvas is an irreversible operation! Would like to proceed?"), Messagebox::ConfirmIcon, Messagebox::YesNoButtons);

	if(msgbox.result() == QDialog::Accepted)
	{
		qApp->setOverrideCursor(Qt::WaitCursor);

		if(sender() == arrange_menu.actions().at(0))
			current_model->rearrangeSchemasInGrid();
		else if(sender() == arrange_menu.actions().at(1))
			current_model->rearrangeTablesHierarchically();
		else
			current_model->rearrangeTablesInSchemas();

		qApp->restoreOverrideCursor();
	}
}

void MainWindow::toggleCompactView()
{
	ModelWidget *model_wgt = nullptr;

	BaseObjectView::setCompactViewEnabled(action_compact_view->isChecked());
	qApp->setOverrideCursor(Qt::WaitCursor);

	for(int idx = 0; idx < models_tbw->count(); idx++)
	{
		model_wgt = dynamic_cast<ModelWidget *>(models_tbw->widget(idx));

		if(action_compact_view->isChecked())
			model_wgt->setAllCollapseMode(BaseTable::ExtAttribsCollapsed);
		else
			model_wgt->setAllCollapseMode(BaseTable::NotCollapsed);

		model_wgt->getDatabaseModel()->setObjectsModified({ ObjectType::Table, ObjectType::ForeignTable,
																												ObjectType::View, ObjectType::Relationship,
																												ObjectType::BaseRelationship, ObjectType::Schema});
	}

	if(current_model)
		current_model->update();

	qApp->restoreOverrideCursor();
}

void MainWindow::toggleLayersWidget(bool show)
{
	setBottomFloatingWidgetPos(layers_cfg_wgt, layers_btn);
	layers_cfg_wgt->setVisible(show);

	changelog_btn->blockSignals(true);
	changelog_btn->setChecked(false);
	changelog_wgt->setVisible(false);
	changelog_btn->blockSignals(false);
}

void MainWindow::toggleChangelogWidget(bool show)
{
	setBottomFloatingWidgetPos(changelog_wgt, changelog_btn);
	changelog_wgt->setVisible(show);

	layers_btn->blockSignals(true);
	layers_btn->setChecked(false);
	layers_cfg_wgt->setVisible(false);
	layers_btn->blockSignals(false);
}

void MainWindow::configureMoreActionsMenu()
{
	if(!current_model)
		return;

	QList<QAction *> actions = current_model->popup_menu.actions();

	more_actions_menu.clear();
	actions.removeOne(current_model->action_new_object);
	actions.removeOne(current_model->action_quick_actions);
	actions.removeOne(current_model->action_edit);
	actions.removeOne(current_model->action_source_code);
	more_actions_menu.addActions(actions);
}

void MainWindow::switchView(MWViewsId view)
{
	switch(view)
	{
		case(ManageView):
			action_manage->toggle();
		break;
		case(DesignView):
			action_design->toggle();
		break;
		case(WelcomeView):
			action_welcome->toggle();
		break;
	}
}

void MainWindow::addExecTabInSQLTool(const QString &sql_cmd)
{
	try
	{
		if(sql_tool_wgt->hasDatabasesBrowsed())
			sql_tool_wgt->addSQLExecutionTab(sql_cmd);
	}
	catch(Exception &e)
	{
		throw Exception(e.getErrorMessage(), e.getErrorCode(), __PRETTY_FUNCTION__, __FILE__, __LINE__, &e);
	}
}

bool MainWindow::hasDbsListedInSQLTool()
{
	return sql_tool_wgt->hasDatabasesBrowsed();
}

void MainWindow::registerRecentModel(const QString &filename)
{
	if(!QFileInfo::exists(filename))
		return;

	recent_models.append(filename);
	updateRecentModelsMenu();
}

void MainWindow::registerRecentModelIcon(const QString &suffix, const QIcon &file_type_icon)
{
	QString fmt_suffix = suffix;

	fmt_suffix.remove(".");
	fmt_suffix = fmt_suffix.simplified();

	if(fmt_suffix.isEmpty() || file_type_icon.isNull() ||
		 GlobalAttributes::DbModelExt.contains(suffix))
		return;

	if(!recent_models_icons.contains(fmt_suffix))
		recent_models_icons[fmt_suffix] = file_type_icon;
}
