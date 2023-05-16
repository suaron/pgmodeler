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

#include "fileselectorwidget.h"
#include "guiutilsns.h"

FileSelectorWidget::FileSelectorWidget(QWidget *parent) : QWidget(parent)
{
	setupUi(this);
	allow_filename_input = read_only = false;
	file_is_mandatory = check_exec_flag = false;
	file_must_exist = false;

	file_dlg.setWindowIcon(QPixmap(GuiUtilsNs::getIconPath("pgmodeler_logo")));

	filename_edt->setReadOnly(true);
	filename_edt->installEventFilter(this);

	warn_ico_lbl = new QLabel(this);
	warn_ico_lbl->setVisible(false);
	warn_ico_lbl->setMinimumSize(filename_edt->height() * 0.75, filename_edt->height() * 0.75);
	warn_ico_lbl->setMaximumSize(warn_ico_lbl->minimumSize());
	warn_ico_lbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	warn_ico_lbl->setScaledContents(true);
	warn_ico_lbl->setPixmap(QPixmap(GuiUtilsNs::getIconPath("alert")));
	warn_ico_lbl->setToolTip(tr("No such file or directory!"));

	connect(sel_file_tb, &QToolButton::clicked, this, &FileSelectorWidget::openFileDialog);
	connect(rem_file_tb, &QToolButton::clicked, this, &FileSelectorWidget::clearSelector);

	connect(filename_edt, &QLineEdit::textChanged, this, [this](const QString &text){
		rem_file_tb->setEnabled(!text.isEmpty());
		validateSelectedFile();
		emit s_selectorChanged(!text.isEmpty());
	});
}

bool FileSelectorWidget::eventFilter(QObject *obj, QEvent *evnt)
{
	if(isEnabled() && evnt->type() == QEvent::MouseButtonPress &&
		 QApplication::mouseButtons() == Qt::LeftButton && obj == filename_edt)
	{
		if(!allow_filename_input && !read_only)
		{
			openFileDialog();
			return true;
		}
	}

	return QWidget::eventFilter(obj, evnt);
}

void FileSelectorWidget::resizeEvent(QResizeEvent *)
{
	warn_ico_lbl->move(filename_edt->width() - warn_ico_lbl->width() - 5,
										 (filename_edt->height() - warn_ico_lbl->height()) / 2);
}

void FileSelectorWidget::setAllowFilenameInput(bool allow_fl_input)
{
	allow_filename_input = allow_fl_input && !read_only;
	filename_edt->setReadOnly(!allow_filename_input);
}

void FileSelectorWidget::setFileMode(QFileDialog::FileMode file_mode)
{
	// Forcing the ExistingFile (single file selection) if multiple file selection is provided
	if(file_mode == QFileDialog::ExistingFiles)
		file_mode = QFileDialog::ExistingFile;

	file_dlg.setFileMode(file_mode);
	validateSelectedFile();
}

void FileSelectorWidget::setAcceptMode(QFileDialog::AcceptMode accept_mode)
{
	file_dlg.setAcceptMode(accept_mode);
}

void FileSelectorWidget::setNameFilters(const QStringList &filters)
{
	file_dlg.setNameFilters(filters);
}

void FileSelectorWidget::setNamePattern(const QString &pattern)
{
	name_regexp.setPattern(pattern);
}

void FileSelectorWidget::setCheckExecutionFlag(bool value)
{
	check_exec_flag = value;
}

void FileSelectorWidget::setFileIsMandatory(bool value)
{
	file_is_mandatory = value;
}

void FileSelectorWidget::setFileMustExist(bool value)
{
	file_must_exist = value;
}

void FileSelectorWidget::setFileDialogTitle(const QString &title)
{
	file_dlg.setWindowTitle(title);
}

void FileSelectorWidget::setSelectedFile(const QString &file)
{
	filename_edt->setText(file);
}

void FileSelectorWidget::setMimeTypeFilters(const QStringList &filters)
{
	file_dlg.setMimeTypeFilters(filters);
}

void FileSelectorWidget::setDefaultSuffix(const QString &suffix)
{
	file_dlg.setDefaultSuffix(suffix);
}

bool FileSelectorWidget::hasWarning()
{
	QString str = warn_ico_lbl->toolTip();
	return !warn_ico_lbl->toolTip().isEmpty();
}

QString FileSelectorWidget::getSelectedFile()
{
	return filename_edt->text();
}

void FileSelectorWidget::clearCustomWarning()
{
	warn_ico_lbl->setToolTip("");
	showWarning();
}

void FileSelectorWidget::setReadOnly(bool value)
{
	read_only = value;
	filename_edt->setReadOnly(value);
	allow_filename_input = false;

	sel_file_tb->setToolTip(value ? tr("Open in file manager") : tr("Select file"));
	rem_file_tb->setVisible(!value);

	if(value)
	{
		disconnect(sel_file_tb, &QToolButton::clicked, this, &FileSelectorWidget::openFileDialog);
		connect(sel_file_tb, &QToolButton::clicked, this, &FileSelectorWidget::openFileExternally);
	}
	else
	{
		connect(sel_file_tb, &QToolButton::clicked, this, &FileSelectorWidget::openFileDialog);
		disconnect(sel_file_tb, &QToolButton::clicked, this, &FileSelectorWidget::openFileExternally);
	}
}

void FileSelectorWidget::setToolTip(const QString &tooltip)
{
	filename_edt->setToolTip(tooltip);
}

void FileSelectorWidget::setCustomWarning(const QString &warn_msg)
{
	warn_ico_lbl->setToolTip(warn_msg);
	showWarning();
}

void FileSelectorWidget::openFileDialog()
{
	filename_edt->clearFocus();
	file_dlg.selectFile(filename_edt->text());

	GuiUtilsNs::restoreFileDialogState(&file_dlg);
	file_dlg.exec();
	GuiUtilsNs::saveFileDialogState(&file_dlg);

	if(file_dlg.result() == QDialog::Accepted && !file_dlg.selectedFiles().isEmpty())
	{
		filename_edt->setText(file_dlg.selectedFiles().at(0));
		emit s_fileSelected(file_dlg.selectedFiles().at(0));
	}
}

void FileSelectorWidget::openFileExternally()
{
	QDesktopServices::openUrl(QUrl("file:///" + filename_edt->text()));
}

void FileSelectorWidget::showWarning()
{
	QPalette pal;
	int padding = 0;
	bool has_warn = !warn_ico_lbl->toolTip().isEmpty();

	warn_ico_lbl->setVisible(has_warn);

	if(has_warn)
	{
		pal.setColor(QPalette::Text, QColor(255, 0, 0));
		padding = warn_ico_lbl->width();
	}
	else
		pal = qApp->palette();

	filename_edt->setStyleSheet(QString("padding: 2px %1px 2px 1px").arg(padding));
	filename_edt->setPalette(pal);
}

void FileSelectorWidget::validateSelectedFile()
{
	QFileInfo fi(filename_edt->text());
	warn_ico_lbl->setToolTip("");

	if(file_is_mandatory && fi.absoluteFilePath().isEmpty())
	{
		if(file_dlg.fileMode() == QFileDialog::Directory)
			warn_ico_lbl->setToolTip(tr("A path to a directory must be provided!"));
		else
			warn_ico_lbl->setToolTip(tr("A path to a file must be provided!"));
	}
	else if(!fi.absoluteFilePath().isEmpty())
	{
		if(fi.exists() && fi.isDir() && file_dlg.fileMode() != QFileDialog::Directory)
			warn_ico_lbl->setToolTip(tr("The provided path is not a file!"));
		else if(fi.exists() && fi.isFile() && file_dlg.fileMode() == QFileDialog::Directory)
			warn_ico_lbl->setToolTip(tr("The provided path is not a directory!"));
		else if(file_must_exist && !fi.exists() /*&& file_dlg.fileMode() != QFileDialog::AnyFile*/)
		{
			if(file_dlg.fileMode() == QFileDialog::Directory)
				warn_ico_lbl->setToolTip(tr("No such directory!"));
			else
				warn_ico_lbl->setToolTip(tr("No such file!"));
		}
		else if(fi.exists())
		{
			// Validating the name pattern against the selected filename
			if(name_regexp.isValid() && !fi.absoluteFilePath().contains(name_regexp))
			{
				if(file_dlg.fileMode() == QFileDialog::Directory)
					warn_ico_lbl->setToolTip(tr("The selected directory is not valid!"));
				else
					warn_ico_lbl->setToolTip(tr("The selected file is not valid!"));
			}
			else if(check_exec_flag && !fi.isDir() && !fi.isExecutable())
				warn_ico_lbl->setToolTip(tr("The selected file is not executable!"));
		}
	}

	showWarning();
}

void FileSelectorWidget::clearSelector()
{
	filename_edt->clear();
	filename_edt->clearFocus();
	rem_file_tb->setEnabled(false);

	emit s_selectorCleared();
	emit s_selectorChanged(false);
}
