#include "batteryquery.h"
#include "network.h"
#include "plot.h"

BatteryQuery::BatteryQuery(QWidget *parent)
	: QMainWindow(parent),
	  socket(new QTcpSocket()),
	  timer(new QTimer()),
	  file(Q_NULLPTR),
	  times(0),
	  xAxisTicker(new QCPAxisTicker()),
	  yAxisTicker(new QCPAxisTicker())
{
	ui.setupUi(this);
	initUi();
	initGraph();
	initActions();
	createConnections();
}

BatteryQuery::~BatteryQuery()
{

}


// ��ʼ������
void BatteryQuery::initUi()
{
	ui.statusBar->showMessage("none");
}

// ��ʼ��ʣ�����ͼ��
void BatteryQuery::initGraph()
{
	xAxisTicker->setTickCount(6);
	ui.customPlot->xAxis->setTicker(xAxisTicker);
	ui.customPlot->xAxis->setLabel(QStringLiteral("����ʱ��/����"));
	ui.customPlot->xAxis->setRange(0, XAXIS_UPPER);

	yAxisTicker->setTickCount(10);
	ui.customPlot->yAxis->setTicker(yAxisTicker);
	ui.customPlot->yAxis->setLabel(QStringLiteral("ʣ�����/%"));
	ui.customPlot->yAxis->setRange(YAXIS_LOWER, YAXIS_UPPER);

}

// ��ʼ��
void BatteryQuery::initActions()
{
	ui.actionOpen->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_O));
	ui.actionClose->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_C));

	ui.actionSave->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
	QObject::connect(ui.actionSave, SIGNAL(triggered()), this, SLOT(handleSaveAs()));

	ui.actionExit->setShortcut(QKeySequence(Qt::ALT + Qt::Key_F4));
	ui.actionAbout->setShortcut(QKeySequence(Qt::ALT + Qt::Key_A));
}


// ��ʼ�������ź����
void BatteryQuery::createConnections()
{
	// ע��
	int id = qRegisterMetaType<QAbstractSocket::SocketState>();

	// ui
	QObject::connect(ui.connectButton, SIGNAL(clicked()), this, SLOT(handleConnectClicked()));
	QObject::connect(ui.disconnectButton, SIGNAL(clicked()), this, SLOT(handleDisconnectClicked()));

	// socket
	QObject::connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(handleSocketStateChanged(QAbstractSocket::SocketState)));
	QObject::connect(socket, SIGNAL(readyRead()), this, SLOT(handleDataReceived()));

	// timer
	QObject::connect(timer, SIGNAL(timeout()), this, SLOT(sendQueryMsg()));

}





// �������
void BatteryQuery::handleConnectClicked()
{
	socket->connectToHost(SERVER_IP, SERVER_TCP_PORT);
	if (socket->state() != QAbstractSocket::ConnectedState && !socket->waitForConnected(WAITING_TIME))
	{
		QMessageBox::warning(this, QStringLiteral("����ʧ��"), QStringLiteral("�޷����ӵ�����������ȷ�ϱ������κͱ�����IP��192.168.1.7����ȷ���á�"));
	}
}

// ����Ͽ�����
void BatteryQuery::handleDisconnectClicked()
{
	QMessageBox::StandardButton result = QMessageBox::question(this, QStringLiteral("��������"), QStringLiteral("�Ƿ�����˴ε���������"));
	qDebug() << result;
	switch (result)
	{
		case QMessageBox::Yes:
			socket->close();
			if (socket->state() != QAbstractSocket::UnconnectedState && !socket->waitForDisconnected(WAITING_TIME))
			{
				socket->abort();
			}
			break;
		case QMessageBox::No:
			break;
		default:
			break;
	}

}



//  socket״̬�����仯
void BatteryQuery::handleSocketStateChanged(QAbstractSocket::SocketState state)
{
	switch (state)
	{
		case QAbstractSocket::UnconnectedState:
		{
			timer->stop();
			ui.connectButton->setEnabled(true);
			ui.disconnectButton->setDisabled(true);
		
			file->close();

			break;
		}
		case QAbstractSocket::HostLookupState:
			break;
		case QAbstractSocket::ConnectingState:
			break;
		case QAbstractSocket::ConnectedState:
		{
			ui.connectButton->setDisabled(true);
			ui.disconnectButton->setEnabled(true);
			if (ui.customPlot->graph())
				ui.customPlot->clearGraphs();
			ui.customPlot->addGraph();
			ui.customPlot->graph(0)->setLineStyle(QCPGraph::lsLine);
			ui.customPlot->graph(0)->setScatterStyle(QCPScatterStyle::ssCross);
			ui.customPlot->graph(0)->setPen(QPen(Qt::red));
			ui.statusBar->showMessage(QDateTime::currentDateTime().toString(QStringLiteral("yyyy��MM��dd�� hhʱmm��ss��")));

			if (file != Q_NULLPTR)
				delete file;

			file = new QFile(QString("./") + QDateTime::currentDateTime().toString(QStringLiteral("yyyy��MM��dd�� hhʱmm��ss��")) + QString(".txt"));
			file->open(QIODevice::WriteOnly);
			file->write(ui.statusBar->currentMessage().append("\n").toUtf8());
			file->write(QStringLiteral("ʱ�䣨min��\t��ѹ��V��").append("\n").toUtf8());

			times = 0;
			sendQueryMsg();

			timer->start(TIMER_INTERVAL);
			
			break;
		}
		case QAbstractSocket::BoundState:
			break;
		case QAbstractSocket::ListeningState:
			break;
		case QAbstractSocket::ClosingState:
			break;
		default:
			break;
	}
}



// ��socket���յ�����
void BatteryQuery::handleDataReceived()
{
	QByteArray data = socket->readAll();
	switch ((quint8)data.at(2))
	{
		case BATTERY_RSP:
			qDebug() << times << (double)data.at(3) / 100 + YAXIS_LOWER;
			if (times * TIME_IN_MINUTE > ui.customPlot->xAxis->range().upper)
				ui.customPlot->xAxis->setRange(0, ui.customPlot->xAxis->range().upper + XAXIS_INCREMENT);
			ui.customPlot->graph()->addData(times * TIME_IN_MINUTE, (double)data.at(3)/100 + YAXIS_LOWER);
			ui.customPlot->replot();

			file->write(QString::number(times * TIME_IN_MINUTE).append("\t\t\t").append(QString::number((double)data.at(3) / 100 + YAXIS_LOWER, 'g', 3)).append("\n").toUtf8());
			times++;
			break;		
		default:
			break;
	}
}


// ��timer��ʱʱ�����͵�����ѯ����
void BatteryQuery::sendQueryMsg()
{
	quint8 queryMsg[] = {0x7E, 0x00, 0x06, 0x36, 0x81};
	socket->write((char *)queryMsg, sizeof(queryMsg));
}


// ���Ϊ
void BatteryQuery::handleSaveAs()
{
	QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("���Ϊ"), ui.statusBar->currentMessage(), QStringLiteral("ͼ���ļ�(*.png)"));
	qDebug() << fileName;
	//if (fileName.endsWith(".txt"))
	//{
	//	QSharedPointer<QCPDataContainer<QCPGraphData>> data = ui.customPlot->graph(0)->data();

	//	QFile file(fileName);
	//	file.open(QIODevice::WriteOnly | QIODevice::Append);
	//	file.write(ui.statusBar->currentMessage().append("\n").toUtf8());
	//	file.write(QStringLiteral("ʱ�䣨min��\t��ѹ��V��").append("\n").toUtf8());
	//	for (quint32 i = 0; i < data->size(); i++)
	//	{
	//		file.write(QString::number((quint16)data->at(i)->mainKey()).append("\t\t\t").append(QString::number(data->at(i)->mainValue(), 'g', 3)).append("\n").toUtf8());
	//	}

	//}
	if (fileName.endsWith(".png"))
	{
		ui.customPlot->savePng(fileName);
	}
}