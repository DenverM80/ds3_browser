/*
 * *****************************************************************************
 *   Copyright 2014-2015 Spectra Logic Corporation. All Rights Reserved.
 *   Licensed under the Apache License, Version 2.0 (the "License"). You may not
 *   use this file except in compliance with the License. A copy of the License
 *   is located at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   or in the "license" file accompanying this file.
 *   This file is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *   CONDITIONS OF ANY KIND, either express or implied. See the License for the
 *   specific language governing permissions and limitations under the License.
 * *****************************************************************************
 */

#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QThreadPool>

#include "lib/logger.h"

#include "main_window.h"
#include "models/session.h"
#include "views/console.h"
#include "views/jobs_view.h"
#include "views/session_dialog.h"
#include "views/session_view.h"

const int MainWindow::CANCEL_JOBS_TIMEOUT_IN_MS = 30000;

MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags)
	: QMainWindow(parent, flags),
	  m_isFinished(false),
	  m_sessionTabs(new QTabWidget(this)),
	  m_jobsView(new JobsView(this))
{
	setWindowTitle("Spectra Logic DS3 Explorer");

	Session* session = CreateSession();
	if (session == NULL)
	{
		// User closed/cancelled the New Session dialog which should
		// result in the application closing.
		m_isFinished = true;
		return;
	}

	CreateMenus();

	setCentralWidget(m_sessionTabs);

	m_jobsDock = new QDockWidget("Jobs", this);
	m_jobsDock->setObjectName("jobs dock");
	m_jobsScroll = new QScrollArea;
	m_jobsScroll->setWidget(m_jobsView);
	m_jobsScroll->setWidgetResizable(true);
	m_jobsDock->setWidget(m_jobsScroll);
	addDockWidget(Qt::BottomDockWidgetArea, m_jobsDock);

	m_consoleDock = new QDockWidget("Log", this);
	m_consoleDock->setObjectName("console dock");
	m_consoleDock->setWidget(Console::Instance());
	addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);

	tabifyDockWidget(m_jobsDock, m_consoleDock);
	setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);

	ReadSettings();
}

int
MainWindow::GetNumActiveJobs() const
{
	int num = 0;
	for (int i = 0; i < m_sessionViews.size(); i++) {
		num += m_sessionViews[i]->GetNumActiveJobs();
	}
	return num;
}

void
MainWindow::closeEvent(QCloseEvent* event)
{
	if (GetNumActiveJobs() > 0) {
		QString title = "Active Jobs In Progress";
		QString msg = "There are active jobs still in progress.  " \
			      "Are you sure wish to cancel those jobs and " \
			      "quit the applcation?";
		QMessageBox::StandardButton ret;
		ret = QMessageBox::warning(this, title, msg,
					   QMessageBox::Ok |
					   QMessageBox::Cancel,
					   QMessageBox::Cancel);
		if (ret == QMessageBox::Ok) {
			CancelActiveJobs();
		} else {
			event->ignore();
			return;
		}
	}

	QSettings settings;
	settings.setValue("mainWindow/geometry", saveGeometry());
	settings.setValue("mainWindow/windowState", saveState());
	QMainWindow::closeEvent(event);
}

void
MainWindow::ReadSettings()
{
	QSettings settings;
	restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
	restoreState(settings.value("mainWindow/windowState").toByteArray());
}

Session*
MainWindow::CreateSession()
{
	SessionDialog sessionDialog;
	if (sessionDialog.exec() == QDialog::Rejected) {
		return NULL;
	}

	Session* session = new Session(sessionDialog.GetSession());
	SessionView* sessionView = new SessionView(session, m_jobsView);
	m_sessionViews << sessionView;
	m_sessionTabs->addTab(sessionView, session->GetHost());

	return session;
}

void
MainWindow::CreateMenus()
{
	m_aboutAction = new QAction(tr("&About"), this);
	connect(m_aboutAction, SIGNAL(triggered()), this, SLOT(About()));

	m_helpMenu = new QMenu(tr("&Help"), this);
	m_helpMenu->addAction(m_aboutAction);

	menuBar()->addMenu(m_helpMenu);
}

void
MainWindow::CancelActiveJobs()
{
	for (int i = 0; i < m_sessionViews.size(); i++) {
		m_sessionViews[i]->CancelActiveJobs();
	}
	// All jobs are currently run via QtConcurrent::run, which uses
	// the global thread pool.  This will need to be modified if certain
	// job tasks are ever switched to using a custom thread pool.
	bool ret = QThreadPool::globalInstance()->waitForDone(CANCEL_JOBS_TIMEOUT_IN_MS);
	if (!ret) {
		LOG_ERROR("Timed out waiting for all jobs to stop");
	}
}

void
MainWindow::About()
{
	QString text = tr("<b>DS3 Explorer</b><br/>Version %1")
				.arg(APP_VERSION);
	QMessageBox::about(this, tr("About DS3 Explorer"), text);
}
