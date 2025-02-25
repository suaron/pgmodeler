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

#include "datamanipulationform.h"
#include "sqlexecutionwidget.h"
#include "guiutilsns.h"
#include "utilsns.h"
#include "utils/plaintextitemdelegate.h"
#include "baseform.h"
#include "widgets/bulkdataeditwidget.h"
#include "widgets/objectstablewidget.h"
#include "databaseexplorerwidget.h"
#include "settings/generalconfigwidget.h"

DataManipulationForm::DataManipulationForm(QWidget * parent, Qt::WindowFlags f): QDialog(parent, f)
{
	QAction *act = nullptr;
	QToolButton *btn = nullptr;
	QFont fnt;

	setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

	for(auto &obj : bnts_parent_wgt->children())
	{
		btn = dynamic_cast<QToolButton *>(obj);
		if(!btn) continue;

		fnt = btn->font();
		fnt.setWeight(QFont::Normal);
		btn->setFont(fnt);
		GuiUtilsNs::createDropShadow(btn, 1, 1, 5);
	}

	table_oid=0;
	filter_hl=new SyntaxHighlighter(filter_txt);
	filter_hl->loadConfiguration(GlobalAttributes::getSQLHighlightConfPath());

	code_compl_wgt=new CodeCompletionWidget(filter_txt);
	code_compl_wgt->configureCompletion(nullptr, filter_hl);

	results_tbw->setItemDelegate(new PlainTextItemDelegate(this, false));
	browse_tabs_tb->setMenu(&fks_menu);

	act = copy_menu.addAction(tr("Copy as text"));
	act->setShortcut(QKeySequence("Ctrl+C"));

	connect(act, &QAction::triggered,	this, [this](){
		SQLExecutionWidget::copySelection(results_tbw, false, false);
		paste_tb->setEnabled(true);
	});

	act = copy_menu.addAction(tr("Copy as CSV"));
	act->setShortcut(QKeySequence("Ctrl+Shift+C"));

	connect(act, &QAction::triggered, this, [this](){
		SQLExecutionWidget::copySelection(results_tbw, false, true);
		paste_tb->setEnabled(true);
	});

	act = paste_menu.addAction(tr("Paste as text"));
	act->setShortcut(QKeySequence("Ctrl+V"));

	connect(act, &QAction::triggered,	this, [this](){
		loadDataFromCsv(true, false);
		paste_tb->setEnabled(false);
	});

	act = paste_menu.addAction(tr("Paste as CSV"));
	act->setShortcut(QKeySequence("Ctrl+Shift+V"));

	connect(act, &QAction::triggered,	this, [this](){
		loadDataFromCsv(true, true);
		paste_tb->setEnabled(false);
	});

	edit_tb->setMenu(&edit_menu);

	action_add = edit_menu.addAction(QIcon(GuiUtilsNs::getIconPath("addrow")), tr("Add row(s)"), this, &DataManipulationForm::addRow, QKeySequence("Ins"));
	action_add->setToolTip(tr("Add empty rows"));

	action_delete = edit_menu.addAction(QIcon(GuiUtilsNs::getIconPath("delrow")), tr("Delete row(s)"), this, &DataManipulationForm::markDeleteOnRows, QKeySequence("Del"));
	action_delete->setToolTip(tr("Mark the selected rows to be deleted"));

	action_bulk_edit = edit_menu.addAction(QIcon(GuiUtilsNs::getIconPath("bulkedit")), tr("Edit cells"));
	action_bulk_edit->setShortcut(QKeySequence("Ctrl+E"));
	action_bulk_edit->setToolTip(tr("Change the values of all selected cells at once"));

	connect(action_bulk_edit, &QAction::triggered, this, [this](){
		GuiUtilsNs::bulkDataEdit(results_tbw);
	});

	action_duplicate = edit_menu.addAction(QIcon(GuiUtilsNs::getIconPath("duprow")), tr("Duplicate row(s)"), this, &DataManipulationForm::duplicateRows, QKeySequence("Ctrl+D"));
	action_duplicate->setToolTip(tr("Duplicate the selected rows"));

	action_clear = edit_menu.addAction(QIcon(GuiUtilsNs::getIconPath("cleartext")), tr("Clear cell(s)"), this, &DataManipulationForm::clearItemsText, QKeySequence("Ctrl+R"));
	action_clear->setToolTip(tr("Clears the items selected on the grid"));

	paste_tb->setMenu(&paste_menu);
	truncate_tb->setMenu(&truncate_menu);
	truncate_menu.addAction(QIcon(GuiUtilsNs::getIconPath("truncate")), tr("Truncate"), this, &DataManipulationForm::truncateTable, QKeySequence("Ctrl+Del"))->setData(QVariant::fromValue<bool>(false));
	truncate_menu.addAction(QIcon(GuiUtilsNs::getIconPath("trunccascade")), tr("Truncate cascade"), this, &DataManipulationForm::truncateTable, QKeySequence("Ctrl+Shift+Del"))->setData(QVariant::fromValue<bool>(true));

	copy_tb->setMenu(&copy_menu);
	refresh_tb->setToolTip(refresh_tb->toolTip() + QString(" (%1)").arg(refresh_tb->shortcut().toString()));
	save_tb->setToolTip(save_tb->toolTip() + QString(" (%1)").arg(save_tb->shortcut().toString()));
	paste_tb->setToolTip(paste_tb->toolTip() + QString(" (%1)").arg(paste_tb->shortcut().toString()));
	export_tb->setToolTip(export_tb->toolTip() + QString(" (%1)").arg(export_tb->shortcut().toString()));
	undo_tb->setToolTip(undo_tb->toolTip() + QString(" (%1)").arg(undo_tb->shortcut().toString()));
	csv_load_tb->setToolTip(csv_load_tb->toolTip() + QString(" (%1)").arg(csv_load_tb->shortcut().toString()));
	filter_tb->setToolTip(filter_tb->toolTip() + QString(" (%1)").arg(filter_tb->shortcut().toString()));
	new_window_tb->setToolTip(new_window_tb->toolTip() + QString(" (%1)").arg(new_window_tb->shortcut().toString()));
	result_info_wgt->setVisible(false);

	//Forcing the splitter that handles the bottom widgets to resize its children to their minimum size
	h_splitter->setSizes({500, 250, 500});
	filter_tbw->setVisible(false);
	csv_load_parent->setVisible(false);

	csv_load_wgt = new CsvLoadWidget(this, false);
	QVBoxLayout *layout = new QVBoxLayout;

	layout->addWidget(csv_load_wgt);
	layout->setContentsMargins(0,0,0,0);
	csv_load_parent->setLayout(layout);
	csv_load_parent->setMinimumSize(csv_load_wgt->minimumSize());

	columns_lst->installEventFilter(this);

	connect(columns_lst, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item){
		if(item->checkState() == Qt::Checked)
			item->setCheckState(Qt::Unchecked);
		else
			item->setCheckState(Qt::Checked);

		toggleColumnDisplay(item);
	});

	connect(select_all_tb, &QToolButton::clicked, this, [this](){
	  setColumnsCheckState(Qt::Checked);
	});

	connect(clear_all_tb, &QToolButton::clicked, this, [this](){
	  setColumnsCheckState(Qt::Unchecked);
	});

	connect(columns_lst, &QListWidget::itemClicked, this, &DataManipulationForm::toggleColumnDisplay);
	connect(csv_load_tb, &QToolButton::toggled, csv_load_parent, &QWidget::setVisible);
	connect(close_btn, &QPushButton::clicked, this, &DataManipulationForm::reject);
	connect(schema_cmb, &QComboBox::currentIndexChanged, this, &DataManipulationForm::listTables);
	connect(hide_views_chk, &QCheckBox::toggled, this, &DataManipulationForm::listTables);
	connect(schema_cmb, &QComboBox::currentIndexChanged, this, &DataManipulationForm::disableControlButtons);
	connect(table_cmb, &QComboBox::currentIndexChanged, this, &DataManipulationForm::disableControlButtons);
	connect(table_cmb, &QComboBox::currentIndexChanged, this, &DataManipulationForm::listColumns);
	connect(table_cmb, &QComboBox::currentIndexChanged, this, &DataManipulationForm::retrieveData);
	connect(refresh_tb, &QToolButton::clicked, this, &DataManipulationForm::retrieveData);
	connect(add_ord_col_tb, &QToolButton::clicked, this, &DataManipulationForm::addSortColumnToList);
	connect(ord_columns_lst, &QListWidget::itemDoubleClicked, this, &DataManipulationForm::removeSortColumnFromList);
	connect(ord_columns_lst, &QListWidget::itemPressed, this, &DataManipulationForm::changeOrderMode);
	connect(rem_ord_col_tb, &QToolButton::clicked, this, &DataManipulationForm::removeSortColumnFromList);
	connect(clear_ord_cols_tb, &QToolButton::clicked, this, &DataManipulationForm::clearSortColumnList);
	connect(results_tbw, &QTableWidget::itemChanged, this, &DataManipulationForm::markUpdateOnRow);
	connect(undo_tb, &QToolButton::clicked, this, &DataManipulationForm::undoOperations);
	connect(save_tb, &QToolButton::clicked, this, &DataManipulationForm::saveChanges);
	connect(ord_columns_lst, &QListWidget::currentRowChanged, this, &DataManipulationForm::enableColumnControlButtons);
	connect(move_down_tb,  &QToolButton::clicked, this, &DataManipulationForm::swapColumns);
	connect(move_up_tb,  &QToolButton::clicked, this, &DataManipulationForm::swapColumns);
	connect(filter_tb,  &QToolButton::toggled, filter_tbw, &QTabWidget::setVisible);
	connect(truncate_tb,  &QToolButton::clicked, this, &DataManipulationForm::truncateTable);
	connect(new_window_tb, &QToolButton::clicked, this, &DataManipulationForm::openNewWindow);

	connect(filter_tb, &QToolButton::toggled, this, [this](bool checked){
		v_splitter->setVisible(checked);

		if(checked)
			filter_txt->setFocus();
	});

	//Using the QueuedConnection here to avoid the "edit: editing failed" when editing and navigating through items using tab key
	connect(results_tbw, &QTableWidget::currentCellChanged, this, &DataManipulationForm::insertRowOnTabPress, Qt::QueuedConnection);
	connect(results_tbw, &QTableWidget::itemPressed, this, &DataManipulationForm::showPopupMenu);

	connect(export_tb, &QToolButton::clicked, this, [this](){
		SQLExecutionWidget::exportResults(results_tbw);
	});

	connect(results_tbw, &QTableWidget::itemSelectionChanged, this, &DataManipulationForm::enableRowControlButtons);

	connect(csv_load_wgt, &CsvLoadWidget::s_csvFileLoaded, this, [this](){
		loadDataFromCsv();
	});

	connect(results_tbw->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int section, Qt::SortOrder sort_order){
		// Applying the sorting on the clicked column when the Control key is pressed
		if(qApp->keyboardModifiers() == Qt::ControlModifier)
			sortResults(section, sort_order);
		else
			selectColumn(section, sort_order);
	});
}

void DataManipulationForm::setAttributes(Connection conn, const QString curr_schema, const QString curr_table, const QString &filter)
{
	try
	{
		tmpl_conn_params=conn.getConnectionParams();

		tmpl_window_title = windowTitle() + QString(" - %1") + conn.getConnectionId(true, true);
		setWindowTitle(tmpl_window_title.arg(""));

		db_name_lbl->setText(conn.getConnectionId(true, true, true));

		schema_cmb->clear();
		listObjects(schema_cmb, { ObjectType::Schema });

		disableControlButtons();
		schema_cmb->setCurrentText(curr_schema);

		if(!filter.isEmpty() && !curr_schema.isEmpty() && !curr_table.isEmpty())
		{
			table_cmb->blockSignals(true);
			table_cmb->setCurrentText(curr_table);
			table_cmb->blockSignals(false);

			listColumns();
			filter_txt->setPlainText(filter);
			retrieveData();
			refresh_tb->setEnabled(true);
		}
		else
			table_cmb->setCurrentText(curr_table);
	}
	catch(Exception &e)
	{
		throw Exception(e.getErrorMessage(), e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}

void DataManipulationForm::reject()
{
  GeneralConfigWidget::saveWidgetGeometry(this);
	QDialog::reject();
}

void DataManipulationForm::clearItemsText()
{
	for(auto &sel : results_tbw->selectedRanges())
	{
		for(int row = sel.topRow(); row <= sel.bottomRow(); row++)
		{
			for(int col = sel.leftColumn(); col <= sel.rightColumn(); col++)
				results_tbw->item(row,col)->setText("");
		}
	}
}

void DataManipulationForm::sortResults(int column, Qt::SortOrder order)
{
	clearSortColumnList();
	ord_column_cmb->setCurrentIndex(column);
	asc_rb->setChecked(order == Qt::SortOrder::AscendingOrder);
	desc_rb->setChecked(order == Qt::SortOrder::DescendingOrder);
	addSortColumnToList();
	retrieveData();
	results_tbw->horizontalHeader()->setSortIndicator(column, order);
	results_tbw->horizontalHeader()->setSortIndicatorShown(true);
}

void DataManipulationForm::selectColumn(int column, Qt::SortOrder order)
{
	/* If the column was clicked but the Control key was not pressed
	 * then whe reset to sorting settings and then apply a column selection */
	clearSortColumnList();

	/* Since the sorting order was changed by clicking the column we revert the sorting order to the previous value (before the click)
	 * so the next time the user uses Ctrl+Click the sort is applied correctly based on the sort indicatior */
	results_tbw->horizontalHeader()->blockSignals(true);
	results_tbw->horizontalHeader()->setSortIndicator(column, order == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder);
	results_tbw->horizontalHeader()->setSortIndicatorShown(false);
	results_tbw->horizontalHeader()->blockSignals(false);

	results_tbw->selectColumn(column);
}

void DataManipulationForm::listTables()
{
	table_cmb->clear();
	csv_load_tb->setChecked(false);

	if(schema_cmb->currentIndex() > 0)
	{
		std::vector<ObjectType> types = { ObjectType::Table, ObjectType::ForeignTable };

		if(!hide_views_chk->isChecked())
			types.push_back(ObjectType::View);

		listObjects(table_cmb, types, schema_cmb->currentText());
	}

	table_lbl->setEnabled(table_cmb->count() > 0);
	table_cmb->setEnabled(table_cmb->count() > 0);
	result_info_wgt->setVisible(false);
	setWindowTitle(tmpl_window_title.arg(""));
}

void DataManipulationForm::listColumns()
{
	Catalog catalog;
	Connection conn=Connection(tmpl_conn_params);

	try
	{
		resetAdvancedControls();
		col_names.clear();
		code_compl_wgt->clearCustomItems();

		if(table_cmb->currentIndex() > 0)
		{
			std::vector<attribs_map> cols;

			catalog.setConnection(conn);
			cols=catalog.getObjectsAttributes(ObjectType::Column, schema_cmb->currentText(), table_cmb->currentText());

			for(auto &col : cols)
			{
				col_names.push_back(col[Attributes::Name]);
				code_compl_wgt->insertCustomItem(col[Attributes::Name], {},
				QPixmap(GuiUtilsNs::getIconPath("column")));
			}

			ord_column_cmb->addItems(col_names);
		}

		add_ord_col_tb->setEnabled(ord_column_cmb->count() > 0);
		filter_tb->setEnabled(ord_column_cmb->count() > 0);
	}
	catch(Exception &e)
	{
		catalog.closeConnection();
		throw Exception(e.getErrorMessage(),e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__,&e);
	}

}

void DataManipulationForm::retrieveData()
{
	if(table_cmb->currentIndex() <= 0)
		return;

	Messagebox msg_box;
	Catalog catalog;
	Connection conn_sql=Connection(tmpl_conn_params),
			conn_cat=Connection(tmpl_conn_params);

	try
	{
		if(!changed_rows.empty())
		{
			msg_box.show(tr("<strong>WARNING: </strong> There are some changed rows waiting the commit! Do you really want to discard them and retrieve the data now?"),
						 Messagebox::AlertIcon, Messagebox::YesNoButtons);

			if(msg_box.result()==QDialog::Rejected)
				return;
		}

		QString query=QString("SELECT * FROM \"%1\".\"%2\"").arg(schema_cmb->currentText()).arg(table_cmb->currentText()),
				prev_tab_name;
		ResultSet res;
		unsigned limit=limit_spb->value();
		ObjectType obj_type = static_cast<ObjectType>(table_cmb->currentData(Qt::UserRole).toUInt());
		std::vector<int> curr_hidden_cols;
		int col_cnt = results_tbw->horizontalHeader()->count();
		QDateTime start_dt = QDateTime::currentDateTime(), end_dt;

		prev_tab_name = curr_table_name;
		curr_table_name = QString("%1.%2").arg(schema_cmb->currentText()).arg(table_cmb->currentText());

		/* Storing the current hidden columns to make them hidden again after retrive data
		 * if the table is the same */
		for(int idx = 0; prev_tab_name == curr_table_name && idx < col_cnt; idx++)
		{
			if(results_tbw->horizontalHeader()->isSectionHidden(idx))
				curr_hidden_cols.push_back(idx);
		}

		//Building the where clause
		if(!filter_txt->toPlainText().trimmed().isEmpty())
			query+=QString(" WHERE ") + filter_txt->toPlainText();

		//Building the order by clause
		if(ord_columns_lst->count() > 0)
		{
			QStringList ord_cols, col;

			query+=QString("\n ORDER BY ");

			for(int idx=0; idx < ord_columns_lst->count(); idx++)
			{
				col=ord_columns_lst->item(idx)->text().split(" ");
				ord_cols.push_back(QString("\"") + col[0] + QString("\" ") + col[1]);
			}

			query+=ord_cols.join(QString(", "));
		}

		//Building the limit clause
		if(limit > 0)
			query+=QString(" LIMIT %1").arg(limit);

		QApplication::setOverrideCursor(Qt::WaitCursor);

		catalog.setConnection(conn_cat);
		conn_sql.connect();
		conn_sql.executeDMLCommand(query, res);

		retrievePKColumns(schema_cmb->currentText(), table_cmb->currentText());
		retrieveFKColumns(schema_cmb->currentText(), table_cmb->currentText());
		SQLExecutionWidget::fillResultsTable(catalog, res, results_tbw, true);

		end_dt = QDateTime::currentDateTime();
		qint64 total_exec = end_dt.toMSecsSinceEpoch() - start_dt.toMSecsSinceEpoch();
		QString exec_time_str = total_exec >= 1000 ? QString("%1 s").arg(total_exec/1000.0) : QString("%1 ms").arg(total_exec);

		edit_tb->setEnabled(true);
		export_tb->setEnabled(results_tbw->rowCount() > 0);
		result_info_wgt->setVisible(results_tbw->rowCount() > 0);
		result_info_lbl->setText(QString("<em>[%1]</em> ").arg(end_dt.toString("hh:mm:ss.zzz")) +
								 tr("Rows returned: <strong>%1</strong> in <em><strong>%2</strong></em> ").arg(results_tbw->rowCount()).arg(exec_time_str) +
								 tr("<em>(Limit: <strong>%1</strong> rows)</em>").arg(limit_spb->value()==0 ? tr("none") : QString::number(limit_spb->value())));

		//Reset the changed rows state
		enableRowControlButtons();
		clearChangedRows();

		//If the table is empty automatically creates a new row
		if(results_tbw->rowCount()==0 && PhysicalTable::isPhysicalTable(obj_type))
			addRow();
		else
			results_tbw->setFocus();

		if(PhysicalTable::isPhysicalTable(obj_type))
			csv_load_tb->setEnabled(!col_names.isEmpty());
		else
		{
			csv_load_tb->setEnabled(false);
			csv_load_tb->setChecked(false);
		}

		conn_sql.close();
		catalog.closeConnection();

		QApplication::restoreOverrideCursor();

		paste_tb->setEnabled(!qApp->clipboard()->text().isEmpty() &&
													PhysicalTable::isPhysicalTable(obj_type) &&
												 !col_names.isEmpty());

		truncate_tb->setEnabled(obj_type == ObjectType::Table &&
														res.getTupleCount() > 0 &&
														!col_names.isEmpty());

		code_compl_wgt->clearCustomItems();
		code_compl_wgt->insertCustomItems(col_names, tr("Column"), ObjectType::Column);

		columns_lst->clear();
		QListWidgetItem *item = nullptr;

		for(auto &col : col_names)
		{
			columns_lst->addItem(col);
			item = columns_lst->item(columns_lst->count() - 1);
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			item->setCheckState(Qt::Checked);
			item->setData(Qt::UserRole, item->checkState());
		}

		//Restoring the visibily of the columns
		results_tbw->horizontalHeader()->blockSignals(true);
		col_cnt = results_tbw->horizontalHeader()->count();

		for(int idx = 0; idx < col_cnt; idx++)
			results_tbw->horizontalHeader()->setSectionHidden(idx, false);

		for(auto &idx : curr_hidden_cols)
		{
			item = columns_lst->item(idx);
			item->setCheckState(Qt::Unchecked);
			item->setData(Qt::UserRole, item->checkState());
			results_tbw->horizontalHeader()->setSectionHidden(idx, true);
		}

		if(!curr_hidden_cols.empty())
			results_tbw->resizeRowsToContents();

		results_tbw->horizontalHeader()->blockSignals(false);
		setWindowTitle(tmpl_window_title.arg(curr_table_name.isEmpty() ? "" : curr_table_name + " / "));
	}
	catch(Exception &e)
	{
		QApplication::restoreOverrideCursor();
		conn_sql.close();
		catalog.closeConnection();
		throw Exception(e.getErrorMessage(), e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}

void DataManipulationForm::disableControlButtons()
{
	refresh_tb->setEnabled(schema_cmb->currentIndex() > 0 && table_cmb->currentIndex() > 0);
	results_tbw->setRowCount(0);
	results_tbw->setColumnCount(0);
	warning_frm->setVisible(false);
	hint_frm->setVisible(false);
	edit_tb->setEnabled(false);
	export_tb->setEnabled(false);
	paste_tb->setEnabled(false);
	truncate_tb->setEnabled(false);
	csv_load_tb->setEnabled(false);
	csv_load_tb->setChecked(false);
	clearChangedRows();
}

void DataManipulationForm::enableRowControlButtons()
{
	QList<QTableWidgetSelectionRange> sel_ranges=results_tbw->selectedRanges();
	bool cols_selected, rows_selected;
	ObjectType obj_type = static_cast<ObjectType>(table_cmb->currentData(Qt::UserRole).toUInt());

	cols_selected = rows_selected = !sel_ranges.isEmpty();

	for(auto &sel_rng : sel_ranges)
	{
		cols_selected &= (sel_rng.columnCount() == results_tbw->columnCount());
		rows_selected &= (sel_rng.rowCount() == results_tbw->rowCount());
	}

	action_delete->setEnabled(cols_selected);
	action_duplicate->setEnabled(cols_selected);
	action_bulk_edit->setEnabled(sel_ranges.count() != 0);
	action_clear->setEnabled(sel_ranges.count() != 0);

	copy_tb->setEnabled(sel_ranges.count() != 0);
	paste_tb->setEnabled(!qApp->clipboard()->text().isEmpty() &&
											 PhysicalTable::isPhysicalTable(obj_type)  &&
											 !col_names.isEmpty());
	browse_tabs_tb->setEnabled((!fk_infos.empty() || !ref_fk_infos.empty()) && sel_ranges.count() == 1 && sel_ranges.at(0).rowCount() == 1);
}

void DataManipulationForm::resetAdvancedControls()
{
	ord_column_cmb->clear();
	ord_columns_lst->clear();
	add_ord_col_tb->setEnabled(false);
	filter_txt->clear();
	asc_rb->setChecked(true);
	clear_ord_cols_tb->setEnabled(false);
}

void DataManipulationForm::addSortColumnToList()
{
	if(ord_column_cmb->count() > 0)
	{
		QString item_text;

		item_text=ord_column_cmb->currentText();
		item_text+=(asc_rb->isChecked() ? QString(" ASC") : QString(" DESC"));

		ord_columns_lst->addItem(item_text);
		ord_column_cmb->removeItem(ord_column_cmb->currentIndex());
		enableColumnControlButtons();
	}
}

void DataManipulationForm::enableColumnControlButtons()
{
	clear_ord_cols_tb->setEnabled(ord_columns_lst->count() > 0);
	add_ord_col_tb->setEnabled(ord_column_cmb->count() > 0);
	rem_ord_col_tb->setEnabled(ord_columns_lst->currentRow() >= 0);
	move_up_tb->setEnabled(ord_columns_lst->count() > 1 && ord_columns_lst->currentRow() > 0);
	move_down_tb->setEnabled(ord_columns_lst->count() > 1 &&
													 ord_columns_lst->currentRow() >= 0 &&
													 ord_columns_lst->currentRow() <= ord_columns_lst->count() - 2);
}

void DataManipulationForm::swapColumns()
{
	int curr_idx=0, new_idx=0;
	QStringList items;

	curr_idx=new_idx=ord_columns_lst->currentRow();

	if(sender()==move_up_tb)
		new_idx--;
	else
		new_idx++;

	for(int i=0; i < ord_columns_lst->count(); i++)
		items.push_back(ord_columns_lst->item(i)->text());

	items.move(curr_idx, new_idx);
	ord_columns_lst->blockSignals(true);
	ord_columns_lst->clear();
	ord_columns_lst->addItems(items);
	ord_columns_lst->blockSignals(false);
	ord_columns_lst->setCurrentRow(new_idx);
}

void DataManipulationForm::loadDataFromCsv(bool load_from_clipboard, bool force_csv_parsing)
{
	QList<QStringList> rows;
	QStringList csv_cols;
	int row_id = 0, col_id = 0;
	CsvDocument csv_doc;

	QApplication::setOverrideCursor(Qt::WaitCursor);
	results_tbw->setUpdatesEnabled(false);

	if(load_from_clipboard)
	{
		if(qApp->clipboard()->text().isEmpty())
			return;

		QString csv_pattern = "(%1)(.)*(%1)(%2)";
		QChar separator = QChar::Tabulation, delimiter;
		QString text = qApp->clipboard()->text();

		if(force_csv_parsing)
		{
			if(text.contains(QRegularExpression(csv_pattern.arg("\"").arg(CsvDocument::Separator))))
				delimiter = '\"';
			else if(text.contains(QRegularExpression(csv_pattern.arg("'").arg(CsvDocument::Separator))))
				delimiter='\'';

			// If one of the patterns matched the buffer we configure the right delimiter for csv buffer
			if(!delimiter.isNull())
				separator = CsvDocument::Separator;
		}

		csv_doc = CsvLoadWidget::loadCsvFromBuffer(text, separator, delimiter, false);
	}
	else
	{
		csv_doc = csv_load_wgt->getCsvDocument();
		csv_cols = csv_doc.getColumnNames();
	}

	/* If there is only one empty row in the grid, this one will
	be removed prior the csv loading */
	if(results_tbw->rowCount()==1)
	{
		bool is_empty=true;

		for(int col=0; col < results_tbw->columnCount(); col++)
		{
			if(!results_tbw->item(0, col)->text().isEmpty())
			{
				is_empty=false;
				break;
			}
		}

		if(is_empty)
			removeNewRows({0});
	}

	for(int csv_row = 0; csv_row < csv_doc.getRowCount(); csv_row++)
	{
		addRow();
		row_id = results_tbw->rowCount() - 1;

		for(int csv_col = 0; csv_col < csv_doc.getColumnCount(); csv_col++)
		{
			if(csv_col > csv_doc.getColumnCount())
				break;

			if((!load_from_clipboard && csv_load_wgt->isColumnsInFirstRow()) ||
				 (load_from_clipboard && !csv_cols.isEmpty()))
			{
				//First we need to get the index of the column by its name
				col_id = col_names.indexOf(csv_cols[csv_col]);

				//If a matching column is not found we add the value at the current position
				if(col_id < 0)
					col_id = csv_col;

				if(col_id >= 0 && col_id < results_tbw->columnCount())
					results_tbw->item(row_id, col_id)->setText(csv_doc.getValue(csv_row, csv_col));
			}
			else if(csv_col < results_tbw->columnCount())
			{
				//Insert the value to the cell in order of appearance
				results_tbw->item(row_id, csv_col)->setText(csv_doc.getValue(csv_row, csv_col));
			}
		}
	}

	results_tbw->setUpdatesEnabled(true);
	QApplication::restoreOverrideCursor();
}

void DataManipulationForm::removeSortColumnFromList()
{
	if(qApp->mouseButtons()==Qt::NoButton || qApp->mouseButtons()==Qt::LeftButton)
	{
		QStringList items=col_names;
		int idx=0;

		ord_columns_lst->takeItem(ord_columns_lst->currentRow());

		while(idx < ord_columns_lst->count())
			items.removeOne(ord_columns_lst->item(idx++)->text());

		ord_column_cmb->clear();
		ord_column_cmb->addItems(items);
		enableColumnControlButtons();
	}
}

void DataManipulationForm::clearSortColumnList()
{
	ord_column_cmb->clear();
	ord_column_cmb->addItems(col_names);
	ord_columns_lst->clear();

	clear_ord_cols_tb->setEnabled(false);
	move_up_tb->setEnabled(false);
	move_down_tb->setEnabled(false);
	add_ord_col_tb->setEnabled(true);
}

void DataManipulationForm::changeOrderMode(QListWidgetItem *item)
{
	if(qApp->mouseButtons()==Qt::RightButton)
	{
		QStringList texts=item->text().split(QString(" "));

		if(texts.size() > 1)
			texts[1]=(texts[1]==QString("ASC") ? QString("DESC") : QString("ASC"));

		item->setText(texts[0] + QString(" ") + texts[1]);
	}
}

void DataManipulationForm::listObjects(QComboBox *combo, std::vector<ObjectType> obj_types, const QString &schema)
{
	Catalog catalog;
	Connection conn=Connection(tmpl_conn_params);

	try
	{
		attribs_map objects;
		QStringList items;
		int idx=0, count=0;

		QApplication::setOverrideCursor(Qt::WaitCursor);

		catalog.setConnection(conn);
		catalog.setQueryFilter(Catalog::ListAllObjects);
		combo->blockSignals(true);
		combo->clear();

		for(auto &obj_type : obj_types)
		{
			objects=catalog.getObjectsNames(obj_type, schema);

			for(auto &attr : objects)
				items.push_back(attr.second);

			items.sort();
			combo->addItems(items);
			count+=items.size();
			items.clear();

			for(; idx < count; idx++)
			{
				combo->setItemIcon(idx, QPixmap(GuiUtilsNs::getIconPath(obj_type)));
				combo->setItemData(idx, enum_t(obj_type));
			}

			idx=count;
		}

		if(combo->count()==0)
			combo->insertItem(0, tr("No objects found"));
		else
			combo->insertItem(0, tr("Found %1 object(s)").arg(combo->count()));

		combo->setCurrentIndex(0);
		combo->blockSignals(false);
		catalog.closeConnection();

		QApplication::restoreOverrideCursor();
	}
	catch(Exception &e)
	{
		catalog.closeConnection();
		throw Exception(e.getErrorMessage(), e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}

void DataManipulationForm::retrievePKColumns(const QString &schema, const QString &table)
{
	Catalog catalog;
	Connection conn=Connection(tmpl_conn_params);

	try
	{
		std::vector<attribs_map> pks, columns;
		ObjectType obj_type=static_cast<ObjectType>(table_cmb->currentData().toUInt());

		table_oid = 0;

		if(obj_type==ObjectType::View)
		{
			warning_frm->setVisible(true);
			warning_lbl->setText(tr("Views can't have their data handled through this grid, this way, all operations are disabled."));
		}
		else
		{
			catalog.setConnection(conn);
			//Retrieving the constraints from catalog using a custom filter to select only primary keys (contype=p)
			pks=catalog.getObjectsAttributes(ObjectType::Constraint, schema, table, {}, {{Attributes::CustomFilter, QString("contype='p'")}});

			warning_frm->setVisible(pks.empty());

			if(pks.empty())
				warning_lbl->setText(tr("The selected table doesn't owns a primary key! Updates and deletes will be performed by considering all columns as primary key. <strong>WARNING:</strong> those operations can affect more than one row."));
			else
				table_oid = pks[0][Attributes::Table].toUInt();
		}

		hint_frm->setVisible(PhysicalTable::isPhysicalTable(obj_type));
		action_add->setEnabled(PhysicalTable::isPhysicalTable(obj_type) && !col_names.empty());
		pk_col_names.clear();

		if(!pks.empty())
		{
			QStringList col_str_ids=Catalog::parseArrayValues(pks[0][Attributes::Columns]);
			std::vector<unsigned> col_ids;

			for(QString id : col_str_ids)
				col_ids.push_back(id.toUInt());

			columns=catalog.getObjectsAttributes(ObjectType::Column, schema, table, col_ids);

			for(auto &col : columns)
				pk_col_names.push_back(col[Attributes::Name]);
		}

		catalog.closeConnection();

		//For tables, even if there is no pk the user can manipulate data
		if(PhysicalTable::isPhysicalTable(obj_type))
			results_tbw->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::AnyKeyPressed);
		else
			results_tbw->setEditTriggers(QAbstractItemView::NoEditTriggers);
	}
	catch(Exception &e)
	{
		catalog.closeConnection();
		throw Exception(e.getErrorMessage(), e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}

void DataManipulationForm::retrieveFKColumns(const QString &schema, const QString &table)
{
	Catalog catalog;
	Connection conn=Connection(tmpl_conn_params);

	try
	{
		QAction *action = nullptr;
		std::vector<attribs_map> fks, ref_fks;
		ObjectType obj_type=static_cast<ObjectType>(table_cmb->currentData().toUInt());
		QString fk_name;

		fks_menu.clear();
		ref_fk_infos.clear();
		fk_infos.clear();

		if(obj_type==ObjectType::View)
			return;

		catalog.setConnection(conn);

		//Retrieving the constraints from catalog using a custom filter to select only foreign keys (contype=f)
		fks=catalog.getObjectsAttributes(ObjectType::Constraint, schema, table, {}, {{Attributes::CustomFilter, QString("contype='f'")}});
		ref_fks=catalog.getObjectsAttributes(ObjectType::Constraint, "", "", {}, {{Attributes::CustomFilter, QString("contype='f' AND cs.confrelid=%1").arg(table_oid)}});

		if(!fks.empty() || !ref_fks.empty())
		{
			std::vector<unsigned> col_ids;
			QMenu *submenu = nullptr;

			attribs_map aux_table, aux_schema;
			QStringList name_list;

			submenu = new QMenu(this);

			QAction *act = submenu->menuAction();
			act->setIcon(QPixmap(GuiUtilsNs::getIconPath("referenced")));
			act->setText(tr("Referenced tables"));
			fks_menu.addAction(act);

			if(fks.empty())
				submenu->addAction(tr("(none)"))->setEnabled(false);

			for(auto &fk : fks)
			{				
				aux_table = catalog.getObjectAttributes(ObjectType::Table, fk[Attributes::RefTable].toUInt());
				aux_schema = catalog.getObjectAttributes(ObjectType::Schema, aux_table[Attributes::Schema].toUInt());
				fk_name = QString("%1.%2.%3")
									.arg(aux_schema[Attributes::Name])
									.arg(aux_table[Attributes::Name])
									.arg(fk[Attributes::Name]);

				//Store the referenced schema and table names
				fk_infos[fk_name][Attributes::RefTable] = aux_table[Attributes::Name];
				fk_infos[fk_name][Attributes::Schema] = aux_schema[Attributes::Name];
				action = submenu->addAction(QPixmap(GuiUtilsNs::getIconPath("table")),
																		QString("%1.%2 (%3)").arg(aux_schema[Attributes::Name])
																													.arg(aux_table[Attributes::Name])
																													.arg(fk[Attributes::Name]), this, &DataManipulationForm::browseReferencedTable);
				action->setData(fk_name);

				col_ids.clear();
				name_list.clear();

				//Storing the source columns in a string
				for(QString id : Catalog::parseArrayValues(fk[Attributes::SrcColumns]))
					col_ids.push_back(id.toUInt());

				for(auto &col : catalog.getObjectsAttributes(ObjectType::Column, schema, table, col_ids))
					name_list.push_back(BaseObject::formatName(col[Attributes::Name]));

				fk_infos[fk_name][Attributes::SrcColumns] = name_list.join(UtilsNs::DataSeparator);

				col_ids.clear();
				name_list.clear();

				//Storing the referenced columns in a string
				for(QString id : Catalog::parseArrayValues(fk[Attributes::DstColumns]))
					col_ids.push_back(id.toUInt());

				for(auto &col : catalog.getObjectsAttributes(ObjectType::Column, aux_schema[Attributes::Name], aux_table[Attributes::Name], col_ids))
					name_list.push_back(BaseObject::formatName(col[Attributes::Name]));

				fk_infos[fk_name][Attributes::DstColumns] = name_list.join(UtilsNs::DataSeparator);
			}

			submenu = new QMenu(this);

			act = submenu->menuAction();
			act->setIcon(QPixmap(GuiUtilsNs::getIconPath("referrer")));
			act->setText(tr("Referrer tables"));
			fks_menu.addAction(act);

			if(ref_fks.empty())
				submenu->addAction(tr("(none)"))->setEnabled(false);

			for(auto &fk : ref_fks)
			{
				col_ids.clear();
				name_list.clear();

				aux_table = catalog.getObjectAttributes(ObjectType::Table, fk[Attributes::Table].toUInt());
				aux_schema = catalog.getObjectAttributes(ObjectType::Schema, aux_table[Attributes::Schema].toUInt());
				fk_name = QString("%1.%2.%3")
									.arg(aux_schema[Attributes::Name])
									.arg(aux_table[Attributes::Name])
									.arg(fk[Attributes::Name]);

				//Storing the source columns in a string
				for(auto &id : Catalog::parseArrayValues(fk[Attributes::SrcColumns]))
					col_ids.push_back(id.toUInt());

				for(auto &col : catalog.getObjectsAttributes(ObjectType::Column, aux_schema[Attributes::Name], aux_table[Attributes::Name], col_ids))
					name_list.push_back(BaseObject::formatName(col[Attributes::Name]));

				action = submenu->addAction(QPixmap(GuiUtilsNs::getIconPath("table")),
																		QString("%1.%2 (%3)").arg(aux_schema[Attributes::Name])
																													.arg(aux_table[Attributes::Name])
																													.arg(fk[Attributes::Name]), this, &DataManipulationForm::browseReferrerTable);
				action->setData(fk_name);

				ref_fk_infos[fk_name][Attributes::SrcColumns] = name_list.join(UtilsNs::DataSeparator);
				ref_fk_infos[fk_name][Attributes::Table] = aux_table[Attributes::Name];
				ref_fk_infos[fk_name][Attributes::Schema] = aux_schema[Attributes::Name];
			}
		}

		catalog.closeConnection();
	}
	catch(Exception &e)
	{
		catalog.closeConnection();
		throw Exception(e.getErrorMessage(), e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}

void DataManipulationForm::markOperationOnRow(OperationId operation, int row)
{
	if(row < results_tbw->rowCount() &&
			(operation==NoOperation || results_tbw->verticalHeaderItem(row)->data(Qt::UserRole)!=OpInsert))
	{
		QTableWidgetItem *item=nullptr, *header_item=results_tbw->verticalHeaderItem(row);
		QString tooltip=tr("This row is marked to be %1");
		QFont fnt=results_tbw->font();
		int marked_cols=0;
		QColor item_fg_colors[3] = {
			ObjectsTableWidget::getTableItemColor(ObjectsTableWidget::AddedItemFgColor),
			ObjectsTableWidget::getTableItemColor(ObjectsTableWidget::UpdatedItemFgColor),
			ObjectsTableWidget::getTableItemColor(ObjectsTableWidget::RemovedItemFgColor) },

			item_bg_colors[3] = {
						ObjectsTableWidget::getTableItemColor(ObjectsTableWidget::AddedItemBgColor),
						ObjectsTableWidget::getTableItemColor(ObjectsTableWidget::UpdatedItemBgColor),
						ObjectsTableWidget::getTableItemColor(ObjectsTableWidget::RemovedItemBgColor) };

		if(operation==OpDelete)
			tooltip=tooltip.arg(tr("deleted"));
		else if(operation==OpUpdate)
			tooltip=tooltip.arg(tr("updated"));
		else if(operation==OpInsert)
			tooltip=tooltip.arg(tr("inserted"));
		else
			tooltip.clear();

		results_tbw->blockSignals(true);

		for(int col=0; col < results_tbw->columnCount(); col++)
		{
			item=results_tbw->item(row, col);

			if(results_tbw->horizontalHeaderItem(col)->data(Qt::UserRole)!=QString("bytea"))
			{
				item->setToolTip(tooltip);

				//Restore the item's font and text when the operation is delete or none
				if(operation==NoOperation || operation==OpDelete)
				{
					item->setFont(fnt);
					item->setText(item->data(Qt::UserRole).toString());
				}

				if(operation==NoOperation)
					//Restore the item's background
					item->setBackground(prev_row_colors[row]);
				else
				{
					//Saves the item's background if it isn't already marked
					if(header_item->data(Qt::UserRole)!=OpDelete &&
							header_item->data(Qt::UserRole)!=OpUpdate)
						prev_row_colors[row]=item->background();

					//Changes the item's background and foreground colors according to the operation
					item->setBackground(item_bg_colors[operation - 1]);
					item->setForeground(item_fg_colors[operation - 1]);
				}

				marked_cols++;
			}
		}

		if(marked_cols > 0)
		{
			auto itr=std::find(changed_rows.begin(), changed_rows.end(), row);

			if(operation==NoOperation && itr!=changed_rows.end())
			{
				changed_rows.erase(std::find(changed_rows.begin(), changed_rows.end(), row));
				prev_row_colors.erase(row);
			}
			else if(operation!=NoOperation && itr==changed_rows.end())
				changed_rows.push_back(row);

			header_item->setData(Qt::UserRole, operation);
			undo_tb->setEnabled(!changed_rows.empty());
			save_tb->setEnabled(!changed_rows.empty());
			std::sort(changed_rows.begin(), changed_rows.end());
		}

		results_tbw->blockSignals(false);
	}
}

void DataManipulationForm::markUpdateOnRow(QTableWidgetItem *item)
{
	if(results_tbw->verticalHeaderItem(item->row())->data(Qt::UserRole)!=OpInsert)
	{
		bool items_changed=false;
		QTableWidgetItem *aux_item=nullptr;
		QFont fnt=item->font();

		//Before mark the row to update it's needed to check if some item was changed
		for(int col=0; col < results_tbw->columnCount(); col++)
		{
			aux_item=results_tbw->item(item->row(), col);
			if(!items_changed && aux_item->text()!=aux_item->data(Qt::UserRole))
			{
				items_changed=true;
				break;
			}
		}

		fnt.setBold(items_changed);
		item->setFont(fnt);
		markOperationOnRow((items_changed ? OpUpdate : NoOperation), item->row());
	}
}

void DataManipulationForm::markDeleteOnRows()
{
	QList<QTableWidgetSelectionRange> sel_ranges=results_tbw->selectedRanges();
	QTableWidgetItem *item=nullptr;
	std::vector<int> ins_rows;

	for(auto &sel_rng : sel_ranges)
	{
		for(int row=sel_rng.topRow(); row <= sel_rng.bottomRow(); row++)
		{
			item=results_tbw->verticalHeaderItem(row);

			if(item->data(Qt::UserRole)==OpInsert)
				ins_rows.push_back(row);
			else
				markOperationOnRow(OpDelete, row);
		}
	}

	removeNewRows(ins_rows);
	results_tbw->clearSelection();
}

void DataManipulationForm::addRow(bool focus_new_row)
{
	int row=results_tbw->rowCount();
	QTableWidgetItem *item=nullptr;

	results_tbw->blockSignals(true);
	results_tbw->insertRow(row);

	for(int col=0; col < results_tbw->columnCount(); col++)
	{
		item=new QTableWidgetItem;

		//bytea (binary data) columns can't be handled this way the new item is disabled
		if(results_tbw->horizontalHeaderItem(col)->data(Qt::UserRole)==QString("bytea"))
		{
			item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
			item->setText(tr("[binary data]"));
		}
		else
			item->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

		results_tbw->setItem(row, col, item);
	}

	results_tbw->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(row + 1)));
	results_tbw->blockSignals(false);

	markOperationOnRow(OpInsert, row);
	item=results_tbw->item(row, 0);
	hint_frm->setVisible(true);

	if(focus_new_row)
	{
		results_tbw->setFocus();
		results_tbw->setCurrentCell(row, 0, QItemSelectionModel::ClearAndSelect);
		results_tbw->editItem(item);
	}
}

void DataManipulationForm::duplicateRows()
{
	QList<QTableWidgetSelectionRange> sel_ranges=results_tbw->selectedRanges();

	if(!sel_ranges.isEmpty())
	{
		for(auto &sel_rng : sel_ranges)
		{
			for(int row=sel_rng.topRow(); row <= sel_rng.bottomRow(); row++)
			{
				addRow(false);

				for(int col=0; col < results_tbw->columnCount(); col++)
				{
					results_tbw->item(results_tbw->rowCount() - 1, col)
							->setText(results_tbw->item(row, col)->text());
				}
			}
		}

		results_tbw->setCurrentItem(results_tbw->item(results_tbw->rowCount() - 1, 0), QItemSelectionModel::ClearAndSelect);
	}
}

void DataManipulationForm::removeNewRows(std::vector<int> ins_rows)
{
	if(!ins_rows.empty())
	{
		unsigned idx=0, cnt=ins_rows.size();
		int row_idx=0;
		std::vector<int>::reverse_iterator itr, itr_end;

		//Mark the rows as no-op to remove their indexes from changed rows set
		for(idx=0; idx < cnt; idx++)
			markOperationOnRow(NoOperation, ins_rows[idx]);

		//Remove the rows
		std::sort(ins_rows.begin(), ins_rows.end());
		while(!ins_rows.empty())
		{
			results_tbw->removeRow(ins_rows.back());
			ins_rows.pop_back();
		}

		//Reorganizing the changed rows vector to avoid row index out-of-bound errors
		row_idx=results_tbw->rowCount() - 1;
		itr=changed_rows.rbegin();
		itr_end=changed_rows.rend();

		while(itr!=itr_end)
		{
			if((*itr) > row_idx)
			{
				(*itr)=row_idx;
				results_tbw->verticalHeaderItem(row_idx)->setText(QString::number(row_idx + 1));
				row_idx--;
			}
			else break;

			itr++;
		}
	}
}

void DataManipulationForm::clearChangedRows()
{
	changed_rows.clear();
	prev_row_colors.clear();
	undo_tb->setEnabled(false);
	save_tb->setEnabled(false);
}

void DataManipulationForm::browseTable(const QString &fk_name, bool browse_ref_tab)
{
	QString value, schema, table;
	DataManipulationForm *data_manip = new DataManipulationForm;
	Connection conn = Connection(tmpl_conn_params);
	QStringList filter, src_cols, ref_cols;

	if(browse_ref_tab)
	{
		src_cols =  pk_col_names;
		ref_cols = ref_fk_infos[fk_name][Attributes::SrcColumns].split(UtilsNs::DataSeparator);
		schema = ref_fk_infos[fk_name][Attributes::Schema];
		table = ref_fk_infos[fk_name][Attributes::Table];
	}
	else
	{
		src_cols =  fk_infos[fk_name][Attributes::SrcColumns].split(UtilsNs::DataSeparator);
		ref_cols = fk_infos[fk_name][Attributes::DstColumns].split(UtilsNs::DataSeparator);
		schema = fk_infos[fk_name][Attributes::Schema];
		table = fk_infos[fk_name][Attributes::RefTable];
	}

	for(QString col_name : src_cols)
	{
		value = results_tbw->item(results_tbw->currentRow(), col_names.indexOf(col_name))->text();

		if(value.isEmpty())
			filter.push_back(QString("%1 IS NULL").arg(ref_cols.front()));
		else
			filter.push_back(QString("%1 = '%2'").arg(ref_cols.front()).arg(value));

		ref_cols.pop_front();
	}

	data_manip->setWindowModality(Qt::NonModal);
	data_manip->setAttribute(Qt::WA_DeleteOnClose, true);
	data_manip->setAttributes(conn, schema, table, filter.join(QString("AND")));

	GuiUtilsNs::resizeDialog(data_manip);
	data_manip->show();
}

void DataManipulationForm::browseReferrerTable()
{
	browseTable(qobject_cast<QAction *>(sender())->data().toString(), true);
}

void DataManipulationForm::browseReferencedTable()
{
	browseTable(qobject_cast<QAction *>(sender())->data().toString(), false);
}

void DataManipulationForm::undoOperations()
{
	QTableWidgetItem *item=nullptr;
	std::vector<int> rows, ins_rows;
	QList<QTableWidgetSelectionRange> sel_range=results_tbw->selectedRanges();

	if(!sel_range.isEmpty())
	{
		for(int row=sel_range[0].topRow(); row <= sel_range[0].bottomRow(); row++)
		{
			if(results_tbw->verticalHeaderItem(row)->data(Qt::UserRole).toUInt()==OpInsert)
				ins_rows.push_back(row);
			else
				rows.push_back(row);
		}
	}
	else
	{
		sel_range.clear();
		rows=changed_rows;
	}

	//Marking rows to be deleted/updated as no-op
	for(auto &row : rows)
	{
		item=results_tbw->verticalHeaderItem(row);
		if(item->data(Qt::UserRole).toUInt()!=OpInsert)
			markOperationOnRow(NoOperation, row);
	}

	//If there is no selection, remove all new rows
	if(sel_range.isEmpty())
	{
		if(results_tbw->rowCount() > 0 &&
				results_tbw->verticalHeaderItem(results_tbw->rowCount()-1)->data(Qt::UserRole)==OpInsert)
		{
			do
			{
				results_tbw->removeRow(results_tbw->rowCount()-1);
				item=results_tbw->verticalHeaderItem(results_tbw->rowCount()-1);
			}
			while(item && item->data(Qt::UserRole)==OpInsert);
		}

		clearChangedRows();
	}
	else
		//Removing just the selected new rows
		removeNewRows(ins_rows);


	results_tbw->clearSelection();
	hint_frm->setVisible(results_tbw->rowCount() > 0);
}

void DataManipulationForm::insertRowOnTabPress(int curr_row, int curr_col, int prev_row, int prev_col)
{
	if(qApp->mouseButtons()==Qt::NoButton &&
			curr_row==0 && curr_col==0 &&
			prev_row==results_tbw->rowCount()-1 && prev_col==results_tbw->columnCount()-1)
		addRow();
}

void DataManipulationForm::saveChanges()
{
#ifdef DEMO_VERSION
#warning "DEMO VERSION: data manipulation save feature disabled warning."
	Messagebox msg_box;
	msg_box.show(tr("Warning"),
				 tr("You're running a demonstration version! The save feature of the data manipulation form is available only in the full version!"),
				 Messagebox::AlertIcon, Messagebox::OkButton);
#else
	int row=0;
	Connection conn=Connection(tmpl_conn_params);

	try
	{
		QString cmd;
		Messagebox msg_box;

		msg_box.show(tr("<strong>WARNING:</strong> Once commited its not possible to undo the changes! Proceed with saving?"),
					 Messagebox::AlertIcon,
					 Messagebox::YesNoButtons);

		if(msg_box.result()==QDialog::Accepted)
		{

			//Forcing the cell editor to be closed by selecting an unexistent cell and clearing the selection
			results_tbw->setCurrentCell(-1,-1, QItemSelectionModel::Clear);

			conn.connect();
			conn.executeDDLCommand(QString("START TRANSACTION"));

			for(unsigned idx=0; idx < changed_rows.size(); idx++)
			{
				row=changed_rows[idx];
				cmd=getDMLCommand(row);
				conn.executeDDLCommand(cmd);
			}

			conn.executeDDLCommand(QString("COMMIT"));
			conn.close();

			changed_rows.clear();
			retrieveData();
			undo_tb->setEnabled(false);
			save_tb->setEnabled(false);
		}
	}
	catch(Exception &e)
	{
		std::map<unsigned, QString> op_names={{ OpDelete, tr("delete") },
										 { OpUpdate, tr("update") },
										 { OpInsert, tr("insert") }};

		QString tab_name=QString("%1.%2")
						 .arg(schema_cmb->currentText())
						 .arg(table_cmb->currentText());

		unsigned op_type=results_tbw->verticalHeaderItem(row)->data(Qt::UserRole).toUInt();

		if(conn.isStablished())
		{
			conn.executeDDLCommand(QString("ROLLBACK"));
			conn.close();
		}

		results_tbw->selectRow(row);
		results_tbw->scrollToItem(results_tbw->item(row, 0));

		throw Exception(Exception::getErrorMessage(ErrorCode::RowDataNotManipulated)
						.arg(op_names[op_type]).arg(tab_name).arg(row + 1).arg(e.getErrorMessage()),
						ErrorCode::RowDataNotManipulated,__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
#endif
}

QString DataManipulationForm::getDMLCommand(int row)
{
	if(row < 0 || row >= results_tbw->rowCount())
		return "";

	QString tab_name=QString("\"%1\".\"%2\"").arg(schema_cmb->currentText()).arg(table_cmb->currentText()),
			upd_cmd=QString("UPDATE %1 SET %2 WHERE %3"),
			del_cmd=QString("DELETE FROM %1 WHERE %2"),
			ins_cmd=QString("INSERT INTO %1(%2) VALUES (%3)"),
			fmt_cmd;
	QTableWidgetItem *item=nullptr;
	unsigned op_type=results_tbw->verticalHeaderItem(row)->data(Qt::UserRole).toUInt();
	QStringList val_list, col_list, flt_list;
	QString col_name, value;
	QVariant data;

	if(op_type==OpDelete || op_type==OpUpdate)
	{
		if(pk_col_names.isEmpty())
		{
			//Considering all columns as pk when the tables doesn't has one (except bytea columns)
			for(int col=0; col < results_tbw->columnCount(); col++)
			{
				if(results_tbw->horizontalHeaderItem(col)->data(Qt::UserRole)!=QString("bytea"))
					pk_col_names.push_back(results_tbw->horizontalHeaderItem(col)->text());
			}
		}

		//Creating the where clause with original column's values
		for(QString pk_col : pk_col_names)
		{
			data = results_tbw->item(row,  col_names.indexOf(pk_col))->data(Qt::UserRole);

			if(data.toString() == SQLExecutionWidget::ColumnNullValue)
				flt_list.push_back(QString("\"%1\" IS NULL").arg(pk_col));
			else
				flt_list.push_back(QString("\"%1\"='%2'").arg(pk_col).arg(data.toString().replace("\'","''")));
		}
	}

	if(op_type==OpDelete)
	{
		fmt_cmd=QString(del_cmd).arg(tab_name).arg(flt_list.join(QString(" AND ")));
	}
	else if(op_type==OpUpdate || op_type==OpInsert)
	{
		fmt_cmd=(op_type==OpUpdate ? upd_cmd : ins_cmd);

		for(int col=0; col < results_tbw->columnCount(); col++)
		{
			item=results_tbw->item(row, col);

			//bytea columns are ignored
			if(results_tbw->horizontalHeaderItem(col)->data(Qt::UserRole)!=QString("bytea"))
			{
				value=item->text();
				col_name=results_tbw->horizontalHeaderItem(col)->text();

				if(op_type==OpInsert || (op_type==OpUpdate && value!=item->data(Qt::UserRole)))
				{
					//Checking if the value is a malformed unescaped value, e.g., {value, value}, {value\}
					if((value.startsWith(UtilsNs::UnescValueStart) && value.endsWith(QString("\\") + UtilsNs::UnescValueEnd)) ||
							(value.startsWith(UtilsNs::UnescValueStart) && !value.endsWith(UtilsNs::UnescValueEnd)) ||
							(!value.startsWith(UtilsNs::UnescValueStart) && !value.endsWith(QString("\\") + UtilsNs::UnescValueEnd) && value.endsWith(UtilsNs::UnescValueEnd)))
						throw Exception(Exception::getErrorMessage(ErrorCode::MalformedUnescapedValue)
										.arg(row + 1).arg(col_name),
										ErrorCode::MalformedUnescapedValue,__PRETTY_FUNCTION__,__FILE__,__LINE__);

					col_list.push_back(QString("\"%1\"").arg(col_name));

					//Empty values as considered as DEFAULT
					if(value.isEmpty())
					{
						value=QString("DEFAULT");
					}
					//Unescaped values will not be enclosed in quotes
					else if(value.startsWith(UtilsNs::UnescValueStart) && value.endsWith(UtilsNs::UnescValueEnd))
					{
						value.remove(0,1);
						value.remove(value.length()-1, 1);
					}
					//Quoting value
					else
					{
						value.replace(QString("\\") + UtilsNs::UnescValueStart, UtilsNs::UnescValueStart);
						value.replace(QString("\\") + UtilsNs::UnescValueEnd, UtilsNs::UnescValueEnd);
						value.replace("\'","''");
						value=QString("E'") + value + QString("'");
					}

					if(op_type==OpInsert)
						val_list.push_back(value);
					else
						val_list.push_back(QString("\"%1\"=%2").arg(col_name).arg(value));
				}
			}
		}

		if(col_list.isEmpty())
			return "";
		else
		{
			if(op_type==OpUpdate)
				fmt_cmd=fmt_cmd.arg(tab_name).arg(val_list.join(QString(", "))).arg(flt_list.join(QString(" AND ")));
			else
				fmt_cmd=fmt_cmd.arg(tab_name).arg(col_list.join(QString(", "))).arg(val_list.join(QString(", ")));
		}
	}

	return fmt_cmd;
}

void DataManipulationForm::resizeEvent(QResizeEvent *event)
{
	Qt::ToolButtonStyle style = Qt::ToolButtonIconOnly;
	QToolButton *btn = nullptr;
	QSize screen_sz = this->screen()->size();

	// If the new dialog height is greater than 60% of the screen height we hide the toolbuttons texts
	if(event->size().height() > screen_sz.height() * 0.70)
		style = Qt::ToolButtonTextUnderIcon;

	if(refresh_tb->toolButtonStyle() != style)
	{
		for(auto obj : bnts_parent_wgt->children())
		{
			btn = qobject_cast<QToolButton *>(obj);

			if(btn)
				btn->setToolButtonStyle(style);
		}
	}
}

void DataManipulationForm::closeEvent(QCloseEvent *)
{
  GeneralConfigWidget::saveWidgetGeometry(this);
}

void DataManipulationForm::setColumnsCheckState(Qt::CheckState state)
{
	QListWidgetItem *item = nullptr;

	for(int idx = 0; idx < columns_lst->count(); idx++)
	{
		item = columns_lst->item(idx);
		item->setCheckState(state);
		toggleColumnDisplay(item);
	}
}

bool DataManipulationForm::eventFilter(QObject *object, QEvent *event)
{
	if(object == columns_lst)
	{
		if((event->type() == QEvent::KeyRelease &&
				dynamic_cast<QKeyEvent *>(event)->key() == Qt::Key_Space))
		{
			toggleColumnDisplay(columns_lst->currentItem());
		}
	}

	return QDialog::eventFilter(object, event);
}

void DataManipulationForm::truncateTable()
{
	try
	{
		QAction *act = dynamic_cast<QAction *>(sender());

		if(DatabaseExplorerWidget::truncateTable(schema_cmb->currentText(), table_cmb->currentText(),
																						 act->data().toBool(), Connection(tmpl_conn_params)))
			retrieveData();
	}
	catch(Exception &e)
	{
		Messagebox msg_box;
		msg_box.show(e);
  }
}

void DataManipulationForm::toggleColumnDisplay(QListWidgetItem *item)
{
	if(!item)
		return;

	if(item->checkState() != static_cast<Qt::CheckState>(item->data(Qt::UserRole).toInt()))
	{
		int idx = 0;
		bool hide = false;

		idx = col_names.indexOf(item->text());
		hide = item->checkState() == Qt::Unchecked;
		results_tbw->horizontalHeader()->setSectionHidden(idx, hide);
		item->setCheckState(hide ? Qt::Unchecked : Qt::Checked);
		item->setData(Qt::UserRole, item->checkState());
		//results_tbw->resizeRowsToContents();
	}
}

void DataManipulationForm::openNewWindow()
{
	DataManipulationForm *data_manip = new DataManipulationForm;
	data_manip->setAttributes(tmpl_conn_params, "");
	data_manip->show();
}

void DataManipulationForm::showPopupMenu()
{
	if(QApplication::mouseButtons()==Qt::RightButton)
	{
		QMenu item_menu;
		QAction *act = nullptr;
		ObjectType obj_type=static_cast<ObjectType>(table_cmb->currentData().toUInt());

		act = copy_menu.menuAction();
		act->setIcon(QIcon(GuiUtilsNs::getIconPath("copy")));
		act->setText(tr("Copy items"));
		item_menu.addAction(act);

		act = paste_menu.menuAction();
		act->setIcon(QIcon(GuiUtilsNs::getIconPath("paste")));
		act->setText(tr("Paste items"));
		act->setEnabled(paste_tb->isEnabled());
		item_menu.addAction(act);

		act = item_menu.addAction(QIcon(GuiUtilsNs::getIconPath("cleartext")), tr("Clear items"), this, &DataManipulationForm::clearItemsText);
		act->setEnabled(!results_tbw->selectedRanges().isEmpty());

		if(obj_type == ObjectType::Table)
		{
			item_menu.addSeparator();
			act = fks_menu.menuAction();
			act->setIcon(browse_tabs_tb->icon());
			act->setText(tr("Browse tables"));
			act->setEnabled(browse_tabs_tb->isEnabled());
			item_menu.addAction(act);

			item_menu.addSeparator();
			item_menu.addAction(action_duplicate);
			item_menu.addAction(action_delete);
			item_menu.addAction(action_bulk_edit);
		}

		item_menu.exec(QCursor::pos());
	}
}
