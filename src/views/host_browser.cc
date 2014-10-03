#include <QDebug>
#include <QMenu>

#include "views/host_browser.h"

HostBrowser::HostBrowser(QWidget* parent, Qt::WindowFlags flags)
	: Browser(parent, flags),
	  m_model(new QFileSystemModel(this))
{
	AddCustomToolBarActions();

	QString rootPath = m_model->myComputer().toString();
	UpdatePathLabel(rootPath);
	m_model->setRootPath(rootPath);
	m_model->setFilter(QDir::AllDirs |
			   QDir::AllEntries |
			   QDir::NoDotAndDotDot |
			   QDir::Hidden);
	m_treeView->setModel(m_model);

	m_treeView->setExpandsOnDoubleClick(false);
	m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	connect(m_treeView, SIGNAL(doubleClicked(const QModelIndex&)),
		this, SLOT(OnModelItemDoubleClick(const QModelIndex&)));

	m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void
HostBrowser::AddCustomToolBarActions()
{
	m_homeAction = new QAction(style()->standardIcon(QStyle::SP_DirHomeIcon),
				   "Home directory", this);
	connect(m_homeAction, SIGNAL(triggered()), this, SLOT(GoToHome()));
	m_toolBar->addAction(m_homeAction);
}

void
HostBrowser::UpdatePathLabel(const QString& path)
{
	m_path->setText(QDir::toNativeSeparators(path));
}

void
HostBrowser::GoToHome()
{
	QString path = QDir::homePath();
	m_treeView->setRootIndex(m_model->index(path));
	UpdatePathLabel(path);
}

void
HostBrowser::GoToParent()
{
	QString parentPath;
	QString currentPath = m_model->filePath(m_treeView->rootIndex());
	if (currentPath == "" || QDir::drives().contains(QFileInfo(currentPath)))
	{
		// Either at the "My Computer" level or the root (of a drive
		// on Windows).
		parentPath = m_model->myComputer().toString();
	}
	else
	{
		parentPath = QFileInfo(currentPath).dir().path();
	}
	m_treeView->setRootIndex(m_model->index(parentPath));
	UpdatePathLabel(parentPath);
}

void
HostBrowser::GoToRoot()
{
	QVariant myComputer = m_model->myComputer();
	m_treeView->setRootIndex(myComputer.toModelIndex());
	UpdatePathLabel(myComputer.toString());
}

void
HostBrowser::OnContextMenuRequested(const QPoint& pos)
{
	QMenu menu;
	QAction uploadAction("Upload", &menu);
	QModelIndex itemUnderCursor = m_treeView->indexAt(pos);
	if (!itemUnderCursor.isValid())
	{
		// User didn't right-click on a row in the tree view
		return;
	}

	menu.addAction(&uploadAction);
	QAction* selectedAction = menu.exec(QCursor::pos());
	if (!selectedAction)
	{
		return;
	}

	if (selectedAction == &uploadAction)
	{
		QList<QString> filesToUpload = GetSelectedFiles();
		qDebug() << "files to upload...";
		foreach(QString file, filesToUpload)
		{
			qDebug() << file;
		}
	}
}

void
HostBrowser::OnModelItemDoubleClick(const QModelIndex& index)
{
	QString path = m_model->filePath(index);
	QDir dir = QDir(path);
	if (m_model->isDir(index) && dir.isReadable())
	{
		m_treeView->setRootIndex(index);
		UpdatePathLabel(path);
	}
}

// Get all selected files/directories for upload to the DS3 system.  This
// does not recursively search directories since this could be called
// during a context menu, or drag/drop, event handler.
QList<QString>
HostBrowser::GetSelectedFiles()
{
	QList<QString> filesToUpload;

	QModelIndexList selectedIndexes = m_treeView->selectionModel()->selectedRows();
	foreach(QModelIndex selectedIndex, selectedIndexes)
	{
		filesToUpload << m_model->filePath(selectedIndex);
	}

	return filesToUpload;
}