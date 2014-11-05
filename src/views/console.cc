/*
 * *****************************************************************************
 *   Copyright 2014 Spectra Logic Corporation. All Rights Reserved.
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

#include "lib/logger.h"
#include "views/console.h"

#ifdef NO_DEBUG
#define DEFAULT_LOG_LEVEL INFO
#else
#define DEFAULT_LOG_LEVEL DEBUG
#endif

const QString Console::LEVEL_COLORS[] = { "Blue", "Black", "darkYellow", "Red" };
Console* Console::s_instance = 0;

Console*
Console::Instance()
{
	if (!s_instance) {
		s_instance = new Console();
	}
	return s_instance;
}

Console::Console(QWidget* parent)
	: QWidget(parent),
	  m_lock(new QMutex),
	  m_logLevel(DEFAULT_LOG_LEVEL)
{
	m_text = new QTextEdit();
	m_text->setReadOnly(true);
	m_layout = new QVBoxLayout(this);
	m_layout->addWidget(m_text);
	setLayout(m_layout);
}

void
Console::Log(Level level, const QString& msg)
{
	if (level < m_logLevel) {
		return;
	}

	QString fullMsg(msg);

	QColor color;
	switch (level) {
	case DEBUG:
		color = QColor("Blue");
		break;
	case INFO:
		color = QColor("Black");
		break;
	case WARNING:
		color = QColor(175, 175, 0);
		break;
	case ERROR:
		color = QColor("Red");
		break;
	};

	m_lock->lock();
	QColor oldColor = m_text->textColor();
	m_text->setTextColor(color);
	m_text->append(fullMsg);
	m_text->setTextColor(oldColor);
	m_lock->unlock();
}