#include "sensor.h"
#include "ui_sensor.h"
#include "RTU_ParameterSetting.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QListView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <vector>

#define MAKEHWORD(a,b) ((uint16_t)(((uint16_t)(a) << 8) | ((uint16_t)(b))))

namespace {
QString buttonStyle(const QString &background)
{
    return QStringLiteral(
               "QPushButton{background:%1;color:#ffffff;border:none;border-radius:4px;padding:8px 18px;font-weight:600;}"
               "QPushButton:hover{filter:brightness(0.95);}"
               "QPushButton:pressed{padding-top:9px;padding-bottom:7px;}")
        .arg(background);
}

QString subtleButtonStyle()
{
    return QStringLiteral("QPushButton{background:#ffffff;color:#2f78e8;border:1px solid #bfd4f6;border-radius:4px;padding:4px 10px;}");
}

void configurePopupWidth(QComboBox *combo)
{
    if (combo == nullptr) {
        return;
    }

    combo->setMinimumWidth(280);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(20);

    int maxWidth = combo->minimumWidth();
    const QFontMetrics fm(combo->font());
    for (int i = 0; i < combo->count(); ++i) {
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(combo->itemText(i)) + 48);
    }

    if (combo->view() != nullptr) {
        combo->view()->setMinimumWidth(maxWidth);
    }
}

QString formatFloatText(float value)
{
    QString text = QString::number(value, 'f', 3);
    while (text.contains(QLatin1Char('.')) &&
           (text.endsWith(QLatin1Char('0')) || text.endsWith(QLatin1Char('.')))) {
        text.chop(1);
    }
    return text;
}

QStringList candidateConfigPaths(const QString &fileName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString currentDir = QDir::currentPath();
    return {
        QDir(appDir).filePath(fileName),
        QDir(appDir).filePath(QStringLiteral("../") + fileName),
        QDir(appDir).filePath(QStringLiteral("../../") + fileName),
        QDir(appDir).filePath(QStringLiteral("../../../") + fileName),
        QDir(appDir).filePath(QStringLiteral("../../../../") + fileName),
        QDir(appDir).filePath(QStringLiteral("../../../../../") + fileName),
        QDir(appDir).filePath(QStringLiteral("../../../../RTU_Parameter_Settings/") + fileName),
        QDir(appDir).filePath(QStringLiteral("../../../../../RTU_Parameter_Settings/") + fileName),
        QDir(appDir).filePath(QStringLiteral("../../../../workdir/RTU_Parameter_Settings/") + fileName),
        QDir(appDir).filePath(QStringLiteral("../../../../../workdir/RTU_Parameter_Settings/") + fileName),
        QDir(currentDir).filePath(fileName),
        QDir(currentDir).filePath(QStringLiteral("../") + fileName),
        QDir(currentDir).filePath(QStringLiteral("../../") + fileName),
        QDir(currentDir).filePath(QStringLiteral("RTU_Parameter_Settings/") + fileName),
        QDir(currentDir).filePath(QStringLiteral("../RTU_Parameter_Settings/") + fileName),
        QDir(currentDir).filePath(QStringLiteral("../../RTU_Parameter_Settings/") + fileName),
        QDir(currentDir).filePath(QStringLiteral("workdir/RTU_Parameter_Settings/") + fileName),
        QDir(currentDir).filePath(QStringLiteral("../workdir/RTU_Parameter_Settings/") + fileName),
        QDir(currentDir).filePath(QStringLiteral("../../workdir/RTU_Parameter_Settings/") + fileName)
    };
}

QVector<int> parseHexSequence(const QString &value)
{
    QVector<int> result;
    const QRegularExpression re(QStringLiteral("([0-9A-Fa-f]+)H"));
    auto it = re.globalMatch(value);
    while (it.hasNext()) {
        const auto match = it.next();
        bool ok = false;
        const int code = match.captured(1).toInt(&ok, 16);
        if (ok) {
            result.push_back(code);
        }
    }
    return result;
}

QMap<int, QString> defaultSensorNameMap()
{
    return {
        {1, QStringLiteral("雨量计 - JD05型翻斗式雨量计")},
        {2, QStringLiteral("水位计 - GL-1型量水位计")},
        {3, QStringLiteral("水堰计 - 磁致伸缩式量水堰计")},
        {4, QStringLiteral("水位计 - 非接触式雷达水位计")},
        {5, QStringLiteral("水位计 - 压力水位计")},
        {6, QStringLiteral("水位计 - OTT CBS气泡式水位计")},
        {7, QStringLiteral("物位计 - GDRD56型雷达物位计")},
        {8, QStringLiteral("流量计 - 堰槽流量计")},
        {9, QStringLiteral("相机 - 科皓相机（标清）")},
        {10, QStringLiteral("水位计 - MPM系列压力水位计")},
        {11, QStringLiteral("水位计 - OTT RLS雷达水位计")},
        {12, QStringLiteral("水位计 - 气泡水位计")},
        {13, QStringLiteral("水位计 - KH.WQX-1型气泡水位计")},
        {14, QStringLiteral("水位计 - 浮子式自收缆水位计")},
        {15, QStringLiteral("传感器 - 墒情传感器")},
        {16, QStringLiteral("传感器 - 风速风向一体式传感器")},
        {17, QStringLiteral("传感器 - 大气温度传感器")},
        {18, QStringLiteral("水位计 - 陶瓷压力式水位计")},
        {19, QStringLiteral("闸位计 - KS10闸位计")},
        {20, QStringLiteral("水位计 - RS485水位计 浮子水位计")},
        {21, QStringLiteral("流量计 - MAG电磁流量计")},
        {22, QStringLiteral("流量计 - 超声波流量计")},
        {23, QStringLiteral("水位计 - 鼎恒水位计")},
        {24, QStringLiteral("传感器 - 水质传感器")},
        {25, QStringLiteral("传感器 - 水质传感器-COD")},
        {26, QStringLiteral("流量计 - TUF-2000流量计")},
        {27, QStringLiteral("流量计 - 多普勒超声波明渠流量计")},
        {28, QStringLiteral("流速仪 - 电波流速仪")},
        {29, QStringLiteral("雷达测流设备（流量与水位）")},
        {30, QStringLiteral("水位计 - 数字压力水位计")},
        {31, QStringLiteral("物位计 - 水文雷达物位计")},
        {32, QStringLiteral("传感器 - OTT DS5X水质传感器")},
        {33, QStringLiteral("传感器 - 气象传感器")},
        {34, QStringLiteral("水位计 - LWT水位计")},
        {35, QStringLiteral("流速仪 - 华儒流速仪")},
        {36, QStringLiteral("传感器 - SWR系列墒情传感器")},
        {37, QStringLiteral("温湿度传感器 - WT1821温湿度传感器")},
        {38, QStringLiteral("相机 - 科皓相机（高清）")},
        {39, QStringLiteral("流量计 - 开封流量计")},
        {40, QStringLiteral("流量计 - 明渠流量计")},
        {41, QStringLiteral("水位计 - 超声波水位计")},
        {42, QStringLiteral("流速仪 - 航征雷达流速仪")},
        {43, QStringLiteral("水位计 - TC401电子水尺")},
        {44, QStringLiteral("水位计 - WYZ-80Z")},
        {45, QStringLiteral("流速仪 - RSS-2-300W雷达流速仪")},
        {46, QStringLiteral("流量计 - 汇中超声流量计")},
        {47, QStringLiteral("流量计 - 奥泰AOJ5000")},
        {48, QStringLiteral("流量计 - IFC110电磁流量计")},
        {49, QStringLiteral("流量计 - 宇星流量计")},
        {50, QStringLiteral("流量计 - 凯思达超声波流量计")},
        {51, QStringLiteral("流量计 - 凯思达电磁流量计")},
        {52, QStringLiteral("流速仪 - RG30雷达流速仪")},
        {53, QStringLiteral("流量计 - 昆仑LDBE-65流量计")},
        {54, QStringLiteral("水位计 - 华禹HYCS-1型超声波液位计")},
        {55, QStringLiteral("流量计 - LSZ型超声波多普勒流量计")},
        {56, QStringLiteral("流量计 - KH.UOCF-301流量计")},
        {57, QStringLiteral("水位计 - 科皓KH.WLX-1雷达水位计")},
        {60, QStringLiteral("水位计 - 金水华禹超声波液位计")},
        {61, QStringLiteral("水质传感器 - 凯米斯水质传感器")},
        {62, QStringLiteral("流量计 - 华聚流量计")},
        {63, QStringLiteral("水质传感器 - 天健创新水质传感器")},
        {64, QStringLiteral("GNSS - 吉欧MIS210")},
        {65, QStringLiteral("MCU - MCU设备")},
        {66, QStringLiteral("渗压计 - 南京葛南GDA1602(1)")},
        {67, QStringLiteral("渗压计 - 北京基康采集模组")},
        {70, QStringLiteral("MCU - STR-5型通讯模块")},
        {72, QStringLiteral("MCU - 峟思MCU通讯模块")},
        {73, QStringLiteral("水堰计 - 峟思磁致伸缩式量水堰计")},
        {74, QStringLiteral("水位计 - 葛南水位计")},
        {75, QStringLiteral("MCU - 科皓单通道MCU")},
        {76, QStringLiteral("MCU - 科皓八通道MCU")},
        {77, QStringLiteral("MCU - 科皓十六通道MCU")},
        {88, QStringLiteral("传感器 - 风速风向传感器")},
        {89, QStringLiteral("传感器 - 温湿度气压传感器")},
        {90, QStringLiteral("传感器 - 探针土壤传感器")},
        {91, QStringLiteral("传感器 - 大气电场仪传感器")},
        {92, QStringLiteral("传感器 - N系列能见度传感器")}
    };
}

QMap<int, QString> defaultPortNameMap()
{
    return {
        {0, QStringLiteral("开关输入S1")},
        {1, QStringLiteral("开关输入S2")},
        {4, QStringLiteral("模拟量A1")},
        {5, QStringLiteral("模拟量A2")},
        {8, QStringLiteral("485串口COM3")},
        {9, QStringLiteral("485串口COM4")},
        {10, QStringLiteral("485串口COM5")},
        {12, QStringLiteral("232串口COM1")},
        {13, QStringLiteral("232串口COM2")},
        {14, QStringLiteral("232串口COM5")},
        {16, QStringLiteral("格雷码串口COM5")}
    };
}

QMap<int, QString> defaultElementNameMap()
{
    return {
        {2, QStringLiteral("瞬时气温")},
        {8, QStringLiteral("气压")},
        {9, QStringLiteral("闸位")},
        {16, QStringLiteral("10CM墒情")},
        {17, QStringLiteral("20CM墒情")},
        {18, QStringLiteral("30CM墒情")},
        {19, QStringLiteral("40CM墒情")},
        {20, QStringLiteral("50CM墒情")},
        {21, QStringLiteral("60CM墒情")},
        {22, QStringLiteral("80CM墒情")},
        {23, QStringLiteral("100CM墒情")},
        {24, QStringLiteral("湿度")},
        {31, QStringLiteral("日降水量")},
        {32, QStringLiteral("当前降水量")},
        {38, QStringLiteral("累计降水量")},
        {39, QStringLiteral("瞬时流量")},
        {40, QStringLiteral("取(排)水口流量1")},
        {41, QStringLiteral("取(排)水口流量2")},
        {42, QStringLiteral("取(排)水口流量3")},
        {43, QStringLiteral("取(排)水口流量4")},
        {44, QStringLiteral("取(排)水口流量5")},
        {48, QStringLiteral("总出库流量")},
        {51, QStringLiteral("风向")},
        {53, QStringLiteral("风速")},
        {54, QStringLiteral("平均流速")},
        {55, QStringLiteral("瞬时流速")},
        {57, QStringLiteral("瞬时河道水位")},
        {58, QStringLiteral("库下水位")},
        {59, QStringLiteral("库上水位")},
        {60, QStringLiteral("水位(取水口1)")},
        {61, QStringLiteral("水位(取水口2)")},
        {62, QStringLiteral("水位(取水口3)")},
        {63, QStringLiteral("水位(取水口4)")},
        {64, QStringLiteral("水位(取水口5)")},
        {65, QStringLiteral("水位(取水口6)")},
        {66, QStringLiteral("水位(取水口7)")},
        {70, QStringLiteral("pH")},
        {71, QStringLiteral("溶解氧")},
        {76, QStringLiteral("氨氮")},
        {88, QStringLiteral("水压1")},
        {89, QStringLiteral("水压2")},
        {90, QStringLiteral("水压3")},
        {91, QStringLiteral("水压4")},
        {92, QStringLiteral("水压5")},
        {93, QStringLiteral("水压6")},
        {94, QStringLiteral("水压7")},
        {95, QStringLiteral("水压8")},
        {104, QStringLiteral("累计流量")},
        {118, QStringLiteral("水量")},
        {120, QStringLiteral("频模、温度")},
        {160, QStringLiteral("渗压水位1")},
        {161, QStringLiteral("渗压水位2")},
        {162, QStringLiteral("渗压水位3")},
        {163, QStringLiteral("渗压水位4")},
        {164, QStringLiteral("渗压水位5")},
        {165, QStringLiteral("渗压水位6")},
        {166, QStringLiteral("渗压水位7")},
        {167, QStringLiteral("渗压水位8")},
        {168, QStringLiteral("渗压水位9")},
        {169, QStringLiteral("土壤温度")},
        {170, QStringLiteral("土壤含水量")},
        {171, QStringLiteral("土壤导电率")},
        {172, QStringLiteral("土壤含盐率")},
        {173, QStringLiteral("电场转速")},
        {174, QStringLiteral("电场值")},
        {175, QStringLiteral("报警等级")},
        {176, QStringLiteral("15s能见度")},
        {177, QStringLiteral("1min能见度")},
        {178, QStringLiteral("10min能见度")},
        {179, QStringLiteral("设备状态")},
        {180, QStringLiteral("天气气象")},
        {243, QStringLiteral("图片数据")},
        {60950, QStringLiteral("X位移")},
        {60951, QStringLiteral("Y位移")},
        {60952, QStringLiteral("Z位移")},
        {65288, QStringLiteral("渗流")},
        {65313, QStringLiteral("垂直高度")},
        {65314, QStringLiteral("频率、温度")}
    };
}

QMap<int, QVector<int>> defaultSensorElementMap()
{
    return {
        {1, {31, 32, 38}},
        {2, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {3, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 65288}},
        {4, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {5, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {6, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {7, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {8, {39, 40, 41, 42, 43, 44, 48, 104}},
        {9, {243}},
        {10, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 160, 161, 162, 163, 164, 165, 166, 167, 168}},
        {11, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {12, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {13, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {14, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {15, {16, 17, 18, 19, 20, 21, 22, 23}},
        {16, {53}},
        {17, {2}},
        {18, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {19, {9}},
        {20, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 160, 161, 162, 163, 164, 165, 166, 167, 168}},
        {21, {39, 40, 41, 42, 43, 44, 48, 104}},
        {22, {39, 40, 41, 42, 43, 44, 48, 104}},
        {23, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {24, {70, 71, 76}},
        {25, {70, 71, 76}},
        {26, {39, 40, 41, 42, 43, 44, 48, 104}},
        {27, {39, 40, 41, 42, 43, 44, 48, 104}},
        {28, {70, 71, 76}},
        {29, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {30, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {31, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {32, {70, 71, 76}},
        {33, {70, 71, 76}},
        {34, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {35, {54, 55}},
        {36, {16, 17, 18, 19, 20, 21, 22, 23}},
        {38, {243}},
        {39, {39, 40, 41, 42, 43, 44, 48, 104}},
        {40, {39, 40, 41, 42, 43, 44, 48, 104}},
        {41, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {42, {54, 55}},
        {43, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {45, {54, 55}},
        {46, {39, 40, 41, 42, 43, 44, 48, 104}},
        {47, {39, 40, 41, 42, 43, 44, 48, 104}},
        {48, {39, 40, 41, 42, 43, 44, 48, 104}},
        {49, {39, 40, 41, 42, 43, 44, 48, 104}},
        {50, {39, 40, 41, 42, 43, 44, 48, 104}},
        {51, {39, 40, 41, 42, 43, 44, 48, 104}},
        {52, {54, 55}},
        {53, {39, 40, 41, 42, 43, 44, 48, 104, 118}},
        {54, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {55, {104, 118}},
        {56, {104, 118}},
        {57, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 65288}},
        {60, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {62, {39, 40, 41, 42, 43, 44, 48, 104}},
        {64, {60950, 60951, 60952, 65313}},
        {65, {88, 89, 90, 91, 92, 93, 94, 95, 65288, 65300}},
        {66, {65314}},
        {67, {65314}},
        {70, {120}},
        {72, {160}},
        {73, {60, 65288}},
        {74, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {75, {120, 160, 161, 162, 163, 164, 165, 166, 167, 168}},
        {76, {120, 160, 161, 162, 163, 164, 165, 166, 167, 168}},
        {77, {120, 160, 161, 162, 163, 164, 165, 166, 167, 168}},
        {80, {57, 58, 59, 60, 61, 62, 63, 64, 65, 66}},
        {88, {51, 53}},
        {89, {2, 24, 8}},
        {90, {169, 170, 171, 172}},
        {91, {173, 174, 175}},
        {92, {176, 177, 178, 179, 180}}
    };
}
}

sensor::sensor(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::sensor)
{
    ui->setupUi(this);
    ui->summaryCard->hide();
    ui->mainSplitter->hide();
    loadPortMappings();
    loadModelMappings();
    loadElementMappings();
    loadAppendMappings();
    buildListPage();
}

sensor::~sensor()
{
    delete ui;
}

void sensor::buildListPage()
{
    m_pageLayout = qobject_cast<QVBoxLayout *>(layout());
    if (m_pageLayout == nullptr) {
        return;
    }

    m_toolbarCard = new QFrame(this);
    m_toolbarCard->setFrameShape(QFrame::StyledPanel);
    QHBoxLayout *toolbarLayout = new QHBoxLayout(m_toolbarCard);
    toolbarLayout->setContentsMargins(16, 14, 16, 14);
    toolbarLayout->setSpacing(12);

    m_readButton = new QPushButton(QStringLiteral("获取传感器参数"), m_toolbarCard);
    m_addButton = new QPushButton(QStringLiteral("+ 新增"), m_toolbarCard);
    m_deleteButton = new QPushButton(QStringLiteral("删除"), m_toolbarCard);

    m_readButton->setStyleSheet(buttonStyle(QStringLiteral("#2f78e8")));
    m_addButton->setStyleSheet(buttonStyle(QStringLiteral("#2f78e8")));
    m_deleteButton->setStyleSheet(buttonStyle(QStringLiteral("#ff4d4f")));

    toolbarLayout->addWidget(m_readButton);
    toolbarLayout->addWidget(m_addButton);
    toolbarLayout->addWidget(m_deleteButton);
    toolbarLayout->addStretch();

    connect(m_readButton, &QPushButton::clicked, this, &sensor::on_set_Button_clicked);
    connect(m_addButton, &QPushButton::clicked, this, &sensor::on_add_sensor_clicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &sensor::on_delete_sensor_clicked);

    QFrame *tableCard = new QFrame(this);
    tableCard->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(0, 0, 0, 0);

    m_sensorTable = new QTableWidget(tableCard);
    m_sensorTable->setColumnCount(6);
    m_sensorTable->setHorizontalHeaderLabels(QStringList()
                                             << QStringLiteral("")
                                             << QStringLiteral("传感器标识")
                                             << QStringLiteral("采集要素")
                                             << QStringLiteral("接入端口")
                                             << QStringLiteral("传感器型号")
                                             << QStringLiteral("操作"));
    m_sensorTable->verticalHeader()->setVisible(false);
    m_sensorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sensorTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sensorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sensorTable->setShowGrid(true);
    m_sensorTable->horizontalHeader()->setStretchLastSection(false);
    m_sensorTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_sensorTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_sensorTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_sensorTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_sensorTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_sensorTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_sensorTable->horizontalHeader()->setMinimumHeight(42);
    m_sensorTable->verticalHeader()->setDefaultSectionSize(56);
    m_sensorTable->setMinimumHeight(420);
    tableLayout->addWidget(m_sensorTable);

    m_pageLayout->addWidget(m_toolbarCard);
    m_pageLayout->addWidget(tableCard, 1);
    refreshSensorTable();
}

void sensor::refreshSensorTable()
{
    if (m_sensorTable == nullptr) {
        return;
    }

    m_sensorTable->clearContents();

    if (m_records.isEmpty()) {
        m_sensorTable->setRowCount(1);
        for (int col = 0; col < m_sensorTable->columnCount(); ++col) {
            QTableWidgetItem *item = new QTableWidgetItem;
            item->setFlags(Qt::NoItemFlags);
            if (col == 2) {
                item->setText(QStringLiteral("暂无数据"));
                item->setTextAlignment(Qt::AlignCenter);
            }
            m_sensorTable->setItem(0, col, item);
        }
        m_sensorTable->setRowHeight(0, 58);
        return;
    }

    m_sensorTable->setRowCount(m_records.size());
    for (int row = 0; row < m_records.size(); ++row) {
        const SensorRecord &record = m_records.at(row);

        QTableWidgetItem *checkItem = new QTableWidgetItem;
        checkItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        checkItem->setCheckState(Qt::Unchecked);
        m_sensorTable->setItem(row, 0, checkItem);
        m_sensorTable->setItem(row, 1, new QTableWidgetItem(record.id));
        m_sensorTable->setItem(row, 2, new QTableWidgetItem(record.element.isEmpty() ? record.name : record.element));
        m_sensorTable->setItem(row, 3, new QTableWidgetItem(record.port));
        m_sensorTable->setItem(row, 4, new QTableWidgetItem(record.model));

        QPushButton *editButton = new QPushButton(QStringLiteral("编辑"), m_sensorTable);
        editButton->setStyleSheet(subtleButtonStyle());
        connect(editButton, &QPushButton::clicked, this, [this, row]() {
            showSensorDialog(row);
        });
        m_sensorTable->setCellWidget(row, 5, editButton);
        m_sensorTable->setRowHeight(row, 56);
    }
}

void sensor::loadPortMappings()
{
    m_portCodeToText.clear();
    m_portTextToCode.clear();

    const QMap<int, QString> defaults = defaultPortNameMap();
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
        m_portCodeToText.insert(it.key(), it.value());
        m_portTextToCode.insert(it.value(), it.key());
    }

    for (const QString &path : candidateConfigPaths(QStringLiteral("sensortypeitemconfig.ini"))) {
        if (!QFileInfo::exists(path)) {
            continue;
        }

        QSettings settings(path, QSettings::IniFormat);
        const QStringList groups = settings.childGroups();
        for (const QString &group : groups) {
            bool groupOk = false;
            const int itemIndex = group.toInt(&groupOk);
            if (!groupOk) {
                continue;
            }

            bool codeOk = false;
            const int portCode = settings.value(group + QStringLiteral("/value")).toString().trimmed().toInt(&codeOk);
            if (!codeOk) {
                continue;
            }

            const int uiIndex = itemIndex + 1;
            QString text;
            if (uiIndex >= 0 && uiIndex < ui->sensor_port->count()) {
                text = ui->sensor_port->itemText(uiIndex).trimmed();
            }
            if (text.isEmpty()) {
                text = QStringLiteral("Port %1").arg(portCode);
            }

            m_portCodeToText.insert(portCode, text);
            m_portTextToCode.insert(text, portCode);
        }
    }
}

void sensor::loadModelMappings()
{
    m_modelCodeToText.clear();
    m_modelTextToCode.clear();
    m_modelCodeToElementCodes = defaultSensorElementMap();

    const QMap<int, QString> defaults = defaultSensorNameMap();
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
        m_modelCodeToText.insert(it.key(), it.value());
        m_modelTextToCode.insert(it.value(), it.key());
    }

    for (const QString &path : candidateConfigPaths(QStringLiteral("sensorinstance.ini"))) {
        if (!QFileInfo::exists(path)) {
            continue;
        }

        QSettings settings(path, QSettings::IniFormat);
        const QStringList groups = settings.childGroups();
        for (const QString &group : groups) {
            bool ok = false;
            const int modelCode = group.toInt(&ok);
            if (!ok) {
                continue;
            }

            const QString name = settings.value(group + QStringLiteral("/name")).toString().trimmed();
            if (!name.isEmpty()) {
                m_modelCodeToText.insert(modelCode, name);
                m_modelTextToCode.insert(name, modelCode);
            }

            const QString instanceCodeString = settings.value(group + QStringLiteral("/instancecode")).toString().trimmed();
            const QVector<int> elementCodes = parseHexSequence(instanceCodeString);
            if (!elementCodes.isEmpty() && !m_modelCodeToElementCodes.contains(modelCode)) {
                m_modelCodeToElementCodes.insert(modelCode, elementCodes);
            }
        }
    }
}

void sensor::loadElementMappings()
{
    m_elementCodeToText.clear();
    m_elementTextToCode.clear();

    const QMap<int, QString> defaults = defaultElementNameMap();
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
        m_elementCodeToText.insert(it.key(), it.value());
        m_elementTextToCode.insert(it.value(), it.key());
    }

    for (const QString &path : candidateConfigPaths(QStringLiteral("instancemap.ini"))) {
        if (!QFileInfo::exists(path)) {
            continue;
        }

        QSettings settings(path, QSettings::IniFormat);
        const QStringList groups = settings.childGroups();
        for (const QString &group : groups) {
            bool ok = false;
            const int code = group.toInt(&ok, 16);
            if (!ok) {
                continue;
            }

            const QString name = settings.value(group + QStringLiteral("/name")).toString().trimmed();
            if (!name.isEmpty()) {
                m_elementCodeToText.insert(code, name);
                m_elementTextToCode.insert(name, code);
            }
        }
    }
}

void sensor::loadAppendMappings()
{
    m_appendCodeToText.clear();
    m_appendTextToCode.clear();

    const QVector<QPair<int, QString>> appendItems = {
        {1, QStringLiteral("80×60")},
        {3, QStringLiteral("160×120")},
        {5, QStringLiteral("320×240")},
        {7, QStringLiteral("640×480")},
        {9, QStringLiteral("980×780")}
    };

    for (const auto &item : appendItems) {
        m_appendCodeToText.insert(item.first, item.second);
        m_appendTextToCode.insert(item.second, item.first);
    }
}

QString sensor::portTextFromCode(int code) const
{
    const QString mapped = m_portCodeToText.value(code);
    if (!mapped.isEmpty()) {
        return mapped;
    }
    return code >= 0 ? QStringLiteral("Port %1").arg(code) : QString();
}

QString sensor::modelTextFromCode(int code) const
{
    const QString mapped = m_modelCodeToText.value(code);
    if (!mapped.isEmpty()) {
        return mapped;
    }
    return code >= 0 ? QStringLiteral("Model %1").arg(code) : QString();
}

QString sensor::elementTextFromCode(int code) const
{
    const QString mapped = m_elementCodeToText.value(code);
    if (!mapped.isEmpty()) {
        return mapped;
    }
    return code > 0 ? QStringLiteral("Element 0x%1").arg(code, 2, 16, QLatin1Char('0')).toUpper() : QString();
}

QString sensor::appendTextFromCode(int code) const
{
    const QString mapped = m_appendCodeToText.value(code);
    if (!mapped.isEmpty()) {
        return mapped;
    }
    return code > 0 ? QString::number(code) : QString();
}

QVector<int> sensor::elementCodesForModel(int modelCode) const
{
    return m_modelCodeToElementCodes.value(modelCode);
}

int sensor::elementCodeFromText(const QString &text) const
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }
    return m_elementTextToCode.value(trimmed, -1);
}

int sensor::portCodeFromText(const QString &text) const
{
    return m_portTextToCode.value(text.trimmed(), -1);
}

int sensor::modelCodeFromText(const QString &text) const
{
    return m_modelTextToCode.value(text.trimmed(), -1);
}

int sensor::appendCodeFromText(const QString &text) const
{
    return m_appendTextToCode.value(text.trimmed(), -1);
}

QString sensor::makeSensorIdentifier(int portCode, int modelCode, int address485) const
{
    return QStringLiteral("%1-%2-%3").arg(portCode).arg(modelCode).arg(address485);
}

SensorConfigFrame sensor::frameFromRecord(const SensorRecord &record) const
{
    SensorConfigFrame frame{};
    frame.sensorCom = static_cast<qint8>(record.portCode);
    frame.sensorType = static_cast<qint16>(record.modelCode);
    frame.sensor485Addr = static_cast<quint8>(record.address485.toUInt());
    frame.sensorInstance[0] = static_cast<quint8>(record.elementCode > 0 ? record.elementCode : 0);
    frame.sensorInstallAngle = record.angle.toFloat();
    frame.sensorBaseValue = record.baseValue.toFloat();
    frame.sensorResolution = record.resolution.toFloat();
    frame.sensorAppendValue = static_cast<quint8>(record.appendCode > 0 ? record.appendCode : 0);
    frame.sensorLowerThreshold = record.lowerAlarm.toFloat();
    frame.sensorUpperThreshold = record.upperAlarm.toFloat();

    SwapDateByte(reinterpret_cast<uint8_t *>(&frame.sensorType), sizeof(frame.sensorType));
    SwapDateByte(reinterpret_cast<uint8_t *>(&frame.sensorInstallAngle), sizeof(frame.sensorInstallAngle));
    SwapDateByte(reinterpret_cast<uint8_t *>(&frame.sensorBaseValue), sizeof(frame.sensorBaseValue));
    SwapDateByte(reinterpret_cast<uint8_t *>(&frame.sensorResolution), sizeof(frame.sensorResolution));
    SwapDateByte(reinterpret_cast<uint8_t *>(&frame.sensorLowerThreshold), sizeof(frame.sensorLowerThreshold));
    SwapDateByte(reinterpret_cast<uint8_t *>(&frame.sensorUpperThreshold), sizeof(frame.sensorUpperThreshold));
    return frame;
}

SensorRecord sensor::recordFromFrame(const SensorConfigFrame &frame, int rowIndex) const
{
    Q_UNUSED(rowIndex);

    SensorConfigFrame decoded = frame;
    SwapDateByte(reinterpret_cast<uint8_t *>(&decoded.sensorType), sizeof(decoded.sensorType));
    SwapDateByte(reinterpret_cast<uint8_t *>(&decoded.sensorInstallAngle), sizeof(decoded.sensorInstallAngle));
    SwapDateByte(reinterpret_cast<uint8_t *>(&decoded.sensorBaseValue), sizeof(decoded.sensorBaseValue));
    SwapDateByte(reinterpret_cast<uint8_t *>(&decoded.sensorResolution), sizeof(decoded.sensorResolution));
    SwapDateByte(reinterpret_cast<uint8_t *>(&decoded.sensorLowerThreshold), sizeof(decoded.sensorLowerThreshold));
    SwapDateByte(reinterpret_cast<uint8_t *>(&decoded.sensorUpperThreshold), sizeof(decoded.sensorUpperThreshold));

    SensorRecord record;
    record.portCode = decoded.sensorCom;
    record.modelCode = decoded.sensorType;
    record.elementCode = decoded.sensorInstance[0];
    record.appendCode = decoded.sensorAppendValue;
    record.address485 = QString::number(decoded.sensor485Addr);
    record.id = makeSensorIdentifier(record.portCode, record.modelCode, decoded.sensor485Addr);
    record.name = elementTextFromCode(record.elementCode);
    record.element = record.name;
    record.port = portTextFromCode(record.portCode);
    record.model = modelTextFromCode(record.modelCode);
    record.angle = formatFloatText(decoded.sensorInstallAngle);
    record.baseValue = formatFloatText(decoded.sensorBaseValue);
    record.resolution = formatFloatText(decoded.sensorResolution);
    record.lowerAlarm = formatFloatText(decoded.sensorLowerThreshold);
    record.upperAlarm = formatFloatText(decoded.sensorUpperThreshold);
    record.extraData = appendTextFromCode(record.appendCode);
    return record;
}

QStringList sensor::comboItems(QComboBox *combo) const
{
    QStringList items;
    if (combo == nullptr) {
        return items;
    }

    for (int i = 0; i < combo->count(); ++i) {
        const QString text = combo->itemText(i).trimmed();
        if (!text.isEmpty()) {
            items << text;
        }
    }
    return items;
}

QString sensor::nextSensorId()
{
    return QStringLiteral("S%1").arg(m_nextSensorIndex++, 3, 10, QLatin1Char('0'));
}

void sensor::showSensorDialog(int editRow)
{
    const bool isEdit = editRow >= 0 && editRow < m_records.size();
    SensorRecord record;
    if (isEdit) {
        record = m_records.at(editRow);
    } else {
        record.id = nextSensorId();
    }

    QDialog dialog(this);
    dialog.setWindowTitle(isEdit ? QStringLiteral("编辑传感器参数") : QStringLiteral("新增传感器参数"));
    dialog.setMinimumSize(1120, 560);

    QVBoxLayout *rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(18, 16, 18, 16);
    rootLayout->setSpacing(16);

    QLabel *titleLabel = new QLabel(dialog.windowTitle(), &dialog);
    titleLabel->setStyleSheet(QStringLiteral("font-size:22px;font-weight:700;color:#111827;"));
    rootLayout->addWidget(titleLabel);

    QWidget *formWidget = new QWidget(&dialog);
    QFormLayout *formLayout = new QFormLayout(formWidget);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setHorizontalSpacing(36);
    formLayout->setVerticalSpacing(20);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rootLayout->addWidget(formWidget);

    QLineEdit *nameEdit = new QLineEdit(record.name, &dialog);
    QComboBox *portCombo = new QComboBox(&dialog);
    QComboBox *modelCombo = new QComboBox(&dialog);
    QComboBox *elementCombo = new QComboBox(&dialog);
    QLineEdit *addressEdit = new QLineEdit(record.address485, &dialog);
    QLineEdit *angleEdit = new QLineEdit(record.angle, &dialog);
    QLineEdit *baseEdit = new QLineEdit(record.baseValue, &dialog);
    QLineEdit *resolutionEdit = new QLineEdit(record.resolution, &dialog);
    QLineEdit *upperEdit = new QLineEdit(record.upperAlarm, &dialog);
    QLineEdit *lowerEdit = new QLineEdit(record.lowerAlarm, &dialog);
    QComboBox *extraCombo = new QComboBox(&dialog);

    const QList<QWidget *> editors = {
        nameEdit, portCombo, modelCombo, elementCombo, addressEdit,
        angleEdit, baseEdit, resolutionEdit, upperEdit, lowerEdit, extraCombo
    };
    for (QWidget *editor : editors) {
        editor->setMinimumHeight(34);
    }

    const auto portCodes = m_portCodeToText.keys();
    for (int portCode : portCodes) {
        portCombo->addItem(portTextFromCode(portCode), portCode);
    }

    const auto modelCodes = m_modelCodeToText.keys();
    for (int modelCode : modelCodes) {
        modelCombo->addItem(modelTextFromCode(modelCode), modelCode);
    }

    const auto appendCodes = m_appendCodeToText.keys();
    extraCombo->addItem(QString(), 0);
    for (int appendCode : appendCodes) {
        extraCombo->addItem(appendTextFromCode(appendCode), appendCode);
    }

    configurePopupWidth(portCombo);
    configurePopupWidth(modelCombo);
    configurePopupWidth(elementCombo);
    configurePopupWidth(extraCombo);

    auto rebuildElementCombo = [this, elementCombo](int modelCode, int currentElementCode) {
        elementCombo->clear();
        const QVector<int> elementCodes = elementCodesForModel(modelCode);
        for (int code : elementCodes) {
            elementCombo->addItem(elementTextFromCode(code), code);
        }

        if (currentElementCode > 0 && elementCombo->findData(currentElementCode) < 0) {
            elementCombo->addItem(elementTextFromCode(currentElementCode), currentElementCode);
        }
    };

    if (record.portCode >= 0) {
        const int index = portCombo->findData(record.portCode);
        if (index >= 0) {
            portCombo->setCurrentIndex(index);
        }
    }

    if (record.modelCode >= 0) {
        const int index = modelCombo->findData(record.modelCode);
        if (index >= 0) {
            modelCombo->setCurrentIndex(index);
        }
    }

    rebuildElementCombo(record.modelCode, record.elementCode);
    if (record.elementCode > 0) {
        const int index = elementCombo->findData(record.elementCode);
        if (index >= 0) {
            elementCombo->setCurrentIndex(index);
        }
    }

    if (record.appendCode > 0) {
        const int index = extraCombo->findData(record.appendCode);
        if (index >= 0) {
            extraCombo->setCurrentIndex(index);
        } else {
            extraCombo->addItem(appendTextFromCode(record.appendCode), record.appendCode);
            extraCombo->setCurrentIndex(extraCombo->count() - 1);
        }
    }

    connect(modelCombo, &QComboBox::currentIndexChanged, &dialog, [=](int index) {
        const int modelCode = modelCombo->itemData(index).toInt();
        rebuildElementCombo(modelCode, 0);
        configurePopupWidth(elementCombo);
    });

    formLayout->addRow(QStringLiteral("传感器名称"), nameEdit);
    formLayout->addRow(QStringLiteral("接入端口"), portCombo);
    formLayout->addRow(QStringLiteral("传感器型号"), modelCombo);
    formLayout->addRow(QStringLiteral("采集要素"), elementCombo);
    formLayout->addRow(QStringLiteral("485地址"), addressEdit);
    formLayout->addRow(QStringLiteral("安装角度"), angleEdit);
    formLayout->addRow(QStringLiteral("基准值"), baseEdit);
    formLayout->addRow(QStringLiteral("分辨率"), resolutionEdit);
    formLayout->addRow(QStringLiteral("上限报警值"), upperEdit);
    formLayout->addRow(QStringLiteral("下限报警值"), lowerEdit);
    formLayout->addRow(QStringLiteral("附加数据"), extraCombo);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal, &dialog);
    QPushButton *saveButton = buttonBox->addButton(QStringLiteral("保存并下发"), QDialogButtonBox::AcceptRole);
    QPushButton *cancelButton = buttonBox->addButton(QDialogButtonBox::Cancel);
    saveButton->setStyleSheet(buttonStyle(QStringLiteral("#2f78e8")));
    cancelButton->setStyleSheet(QStringLiteral("QPushButton{padding:8px 18px;}"));
    rootLayout->addWidget(buttonBox);

    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(saveButton, &QPushButton::clicked, &dialog, [&]() {
        const int portCode = portCombo->currentData().toInt();
        const int modelCode = modelCombo->currentData().toInt();
        const int elementCode = elementCombo->currentData().toInt();
        const int appendCode = extraCombo->currentData().toInt();
        const int address485 = addressEdit->text().trimmed().toInt();

        if (nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("请输入传感器名称"));
            return;
        }
        if (portCombo->currentIndex() < 0) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("请选择接入端口"));
            return;
        }
        if (modelCombo->currentIndex() < 0) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("请选择传感器型号"));
            return;
        }
        if (elementCombo->currentIndex() < 0) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("请选择采集要素"));
            return;
        }
        if (address485 < 0 || address485 > 255) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("485地址需在0到255之间"));
            return;
        }

        record.name = nameEdit->text().trimmed();
        record.port = portCombo->currentText().trimmed();
        record.model = modelCombo->currentText().trimmed();
        record.element = elementCombo->currentText().trimmed();
        record.address485 = QString::number(address485);
        record.angle = angleEdit->text().trimmed();
        record.baseValue = baseEdit->text().trimmed();
        record.resolution = resolutionEdit->text().trimmed();
        record.upperAlarm = upperEdit->text().trimmed();
        record.lowerAlarm = lowerEdit->text().trimmed();
        record.extraData = extraCombo->currentText().trimmed();
        record.portCode = portCode;
        record.modelCode = modelCode;
        record.elementCode = elementCode;
        record.appendCode = appendCode;
        record.id = makeSensorIdentifier(record.portCode, record.modelCode, address485);

        if (isEdit) {
            m_records[editRow] = record;
        } else {
            m_records.push_back(record);
        }

        refreshSensorTable();
        sendSensorConfigWrite();
        dialog.accept();
    });

    dialog.exec();
}

void sensor::on_add_sensor_clicked()
{
    showSensorDialog();
}

void sensor::requestSensorParamRead()
{
    uint32_t pos = 0;
    m_RTU_ParameterSetting->isbasedate = false;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_sendmessage.messageByte));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage), AFN_READBASEPARAM, STX, 8, MTYPE_DOWN);
    pos += 22;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ENQ;
    pos++;
    const uint16_t crcValue = cal_crc16(m_RTU_ParameterSetting->m_sendmessage.messageByte, pos);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos + 1] = (crcValue & 0xff);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ((crcValue >> 8) & 0xff);
    pos += 2;
    m_RTU_ParameterSetting->sMessageLen = pos;
    m_RTU_ParameterSetting->sendmessager_lock.unlock();
    m_RTU_ParameterSetting->m_Device_connection->sendmessage();
}

void sensor::sendSensorConfigWrite()
{
    uint32_t pos = 0;
    m_RTU_ParameterSetting->sendmessager_lock.lock();
    memset(m_RTU_ParameterSetting->m_sendmessage.messageByte, 0, sizeof(m_RTU_ParameterSetting->m_sendmessage.messageByte));

    const int payloadLen = 1 + 1 + static_cast<int>(m_records.size()) * static_cast<int>(sizeof(SensorConfigFrame));
    Message_MainbadyEncode(&(m_RTU_ParameterSetting->m_sendmessage),
                           AFN_BASEPARAM,
                           STX,
                           static_cast<int16_t>(payloadLen + 8),
                           MTYPE_DOWN);
    pos += 22;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = IDCODE_SENSORCONFIG;
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos++] = static_cast<uint8_t>(m_records.size());

    for (const SensorRecord &record : std::as_const(m_records)) {
        const SensorConfigFrame frame = frameFromRecord(record);
        memcpy(m_RTU_ParameterSetting->m_sendmessage.messageByte + pos, &frame, sizeof(SensorConfigFrame));
        pos += sizeof(SensorConfigFrame);
    }

    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ENQ;
    pos++;
    const uint16_t crcValue = cal_crc16(m_RTU_ParameterSetting->m_sendmessage.messageByte, pos);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos + 1] = (crcValue & 0xff);
    m_RTU_ParameterSetting->m_sendmessage.messageByte[pos] = ((crcValue >> 8) & 0xff);
    pos += 2;
    m_RTU_ParameterSetting->sMessageLen = pos;
    m_RTU_ParameterSetting->sendmessager_lock.unlock();
    m_RTU_ParameterSetting->m_Device_connection->sendmessage();
}

void sensor::on_set_Button_clicked()
{
    requestSensorParamRead();
}

void sensor::on_delete_sensor_clicked()
{
    if (m_sensorTable == nullptr || m_records.isEmpty()) {
        return;
    }

    int targetRow = -1;
    for (int row = 0; row < m_sensorTable->rowCount() && row < m_records.size(); ++row) {
        QTableWidgetItem *checkItem = m_sensorTable->item(row, 0);
        if (checkItem != nullptr && checkItem->checkState() == Qt::Checked) {
            targetRow = row;
            break;
        }
    }

    if (targetRow < 0) {
        targetRow = m_sensorTable->currentRow();
    }
    if (targetRow < 0 || targetRow >= m_records.size()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请选择要删除的传感器"));
        return;
    }

    m_records.removeAt(targetRow);
    refreshSensorTable();
    sendSensorConfigWrite();
}

void sensor::handleSensorInfo()
{
    m_RTU_ParameterSetting->recivemessage_lock.lock();

    const uint16_t dataLen = MAKEHWORD(*(m_RTU_ParameterSetting->m_recivemessage.messageByte + 11),
                                       *(m_RTU_ParameterSetting->m_recivemessage.messageByte + 12)) + 15;
    const std::vector<uint8_t> payload(m_RTU_ParameterSetting->m_recivemessage.messageByte + 22,
                                       m_RTU_ParameterSetting->m_recivemessage.messageByte + dataLen - 1);

    int index = 0;
    while (index < static_cast<int>(payload.size())) {
        const uint8_t tag = payload.at(index);
        if (tag == IDCODE_SENSORCONFIG) {
            break;
        }
        if (index + 1 >= static_cast<int>(payload.size())) {
            break;
        }

        if (tag == 0x1E) {
            const int len = payload.at(index + 1);
            index += 2 + len;
        } else {
            const int len = payload.at(index + 1) >> 3;
            index += 2 + len;
        }
    }

    QVector<SensorRecord> records;
    if (index < static_cast<int>(payload.size()) && payload.at(index) == IDCODE_SENSORCONFIG) {
        ++index;
        if (index < static_cast<int>(payload.size())) {
            const int sensorCount = payload.at(index++);
            const int expectedBytes = sensorCount * static_cast<int>(sizeof(SensorConfigFrame));
            if (static_cast<int>(payload.size()) - index >= expectedBytes) {
                records.reserve(sensorCount);
                for (int row = 0; row < sensorCount; ++row) {
                    SensorConfigFrame frame{};
                    memcpy(&frame, payload.data() + index, sizeof(SensorConfigFrame));
                    index += sizeof(SensorConfigFrame);
                    records.push_back(recordFromFrame(frame, row));
                }
            }
        }
    }

    m_RTU_ParameterSetting->recivemessage_lock.unlock();

    m_records = records;
    m_nextSensorIndex = m_records.size() + 1;
    refreshSensorTable();
}
