#include "ProtobufGUI.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QProcess>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QScreen>
#include <QLabel>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QRegularExpressionValidator>
#include <QDir>

#include <windows.h>

ProtobufGUI::ProtobufGUI(QWidget *parent)
    : QMainWindow(parent), libHandle(nullptr)
{
    setWindowTitle("Protobuf Compiler and Serializer GUI");
    setupUI();
    connectSignalsAndSlots();
}

ProtobufGUI::~ProtobufGUI()
{
    if (libHandle) {
        FreeLibrary(static_cast<HMODULE>(libHandle));
    }
}

void ProtobufGUI::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    QVBoxLayout *leftLayout = new QVBoxLayout();
    QVBoxLayout *rightLayout = new QVBoxLayout();

    protoFileButton = new QPushButton("Select .proto file", this);
    protocLocationButton = new QPushButton("Select protoc location", this);
    outputFolderButton = new QPushButton("Select output folder", this);
    compileButton = new QPushButton("Compile Proto", this);
    serializeButton = new QPushButton("Serialize Message", this);
    serializeButton->setEnabled(false);

    leftLayout->addWidget(protoFileButton);
    leftLayout->addWidget(protocLocationButton);
    leftLayout->addWidget(outputFolderButton);
    leftLayout->addWidget(compileButton);
    leftLayout->addWidget(serializeButton);
    leftLayout->addStretch();

    protoContentEdit = new QTextEdit(this);
    protoContentEdit->setPlaceholderText("Enter your .proto content here...");

    generatedCodeEdit = new QPlainTextEdit(this);
    generatedCodeEdit->setReadOnly(true);

    fieldInputsScrollArea = new QScrollArea(this);
    fieldInputsScrollArea->setWidgetResizable(true);
    fieldInputsWidget = new QWidget(fieldInputsScrollArea);
    fieldInputsScrollArea->setWidget(fieldInputsWidget);

    serializedOutputEdit = new QPlainTextEdit(this);
    serializedOutputEdit->setReadOnly(true);

    QWidget *fieldInputsContainer = new QWidget(this);
    QVBoxLayout *fieldInputsLayout = new QVBoxLayout(fieldInputsContainer);
    fieldInputsLayout->addWidget(fieldInputsScrollArea);
    fieldInputsLayout->setContentsMargins(0, 0, 0, 0);

    rightLayout->addWidget(protoContentEdit, 1);
    rightLayout->addWidget(generatedCodeEdit, 1);
    rightLayout->addWidget(fieldInputsContainer, 1);
    rightLayout->addWidget(serializedOutputEdit, 1);

    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(rightLayout, 3);

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int width = screenGeometry.width() * 0.7;
    int height = screenGeometry.height() * 0.8;
    resize(width, height);
}

void ProtobufGUI::connectSignalsAndSlots() {
    connect(protoFileButton, &QPushButton::clicked, this, &ProtobufGUI::selectProtoFile);
    connect(protocLocationButton, &QPushButton::clicked, this, &ProtobufGUI::selectProtocLocation);
    connect(outputFolderButton, &QPushButton::clicked, this, &ProtobufGUI::selectOutputFolder);
    connect(compileButton, &QPushButton::clicked, this, &ProtobufGUI::compileProto);
    connect(serializeButton, &QPushButton::clicked, this, &ProtobufGUI::serializeMessage);
}

void ProtobufGUI::selectProtoFile() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select .proto file", "", "Proto Files (*.proto)");
    if (!filePath.isEmpty()) {
        loadProtoFile(filePath);
    }
}

void ProtobufGUI::selectProtocLocation() {
    protocPath = QFileDialog::getOpenFileName(this, "Select protoc executable", "", "Protoc Executable (protoc.exe)");
}

void ProtobufGUI::selectOutputFolder() {
    outputFolderPath = QFileDialog::getExistingDirectory(this, "Select Output Folder");
    if (!outputFolderPath.isEmpty()) {
        outputFolderButton->setText("Output: " + QDir(outputFolderPath).dirName());
    }
}

void ProtobufGUI::compileProto() {
    if (!validateInputs()) return;

    if (outputFolderPath.isEmpty()) {
        tempDir.reset(new QTemporaryDir());
        if (!tempDir->isValid()) {
            showError("Failed to create temporary directory.");
            return;
        }
    }

    if (!writeProtoFile()) return;
    if (!compileProtoFile()) return;
    if (!readGeneratedCode()) return;

    parseGeneratedCode();

    compileButton->setEnabled(false);
    serializeButton->setEnabled(true);
    showInfo("Proto file compiled successfully. You can now serialize.");
}

void ProtobufGUI::updateFieldInputs() {
    QLayout* layout = fieldInputsWidget->layout();
    if (layout) {
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete layout;
    }

    QVBoxLayout* newLayout = new QVBoxLayout(fieldInputsWidget);
    fieldInputs.clear();

    for (const auto &field : messageFields) {
        QHBoxLayout *fieldLayout = new QHBoxLayout();
        QLabel *label = new QLabel(QString("%1 (%2):").arg(field.name, field.type), fieldInputsWidget);
        fieldLayout->addWidget(label);

        QLineEdit *input = new QLineEdit(fieldInputsWidget);
        if (field.type == "integer") {
            input->setText("0");
            input->setValidator(new QIntValidator(input));
        } else if (field.type == "float") {
            input->setText("0.0");
            input->setValidator(new QDoubleValidator(input));
        } else if (field.type == "boolean") {
            input->setText("false");
            input->setValidator(new QRegularExpressionValidator(QRegularExpression("true|false"), input));
        } else {
            input->setText("");
        }

        fieldLayout->addWidget(input);
        newLayout->addLayout(fieldLayout);
        fieldInputs[field.name] = input;
    }

    newLayout->addStretch();
    fieldInputsWidget->setLayout(newLayout);
}

void ProtobufGUI::parseGeneratedCode() {
    QString code = generatedCodeEdit->toPlainText();
    QStringList lines = code.split('\n');

    messageFields.clear();
    bool inAccessorsSection = false;
    QRegularExpression fieldRegex("^\\s*//\\s*(\\w+)\\s+(\\w+)\\s*=\\s*(\\d+);");

    for (const QString& line : lines) {
        if (line.contains("// accessors -------------------------------------------------------")) {
            inAccessorsSection = true;
            continue;
        }

        if (inAccessorsSection) {
            if (line.contains("// @@protoc_insertion_point(class_scope:")) {
                break;  // End of accessors section
            }

            QRegularExpressionMatch match = fieldRegex.match(line);
            if (match.hasMatch()) {
                MessageField field;
                field.type = match.captured(1);
                field.name = match.captured(2);
                field.number = match.captured(3).toInt();

                if (field.type == "int32" || field.type == "int64") {
                    field.type = "integer";
                } else if (field.type == "float" || field.type == "double") {
                    field.type = "float";
                } else if (field.type == "bool") {
                    field.type = "boolean";
                }

                messageFields.append(field);
            }
        }
    }

    if (messageFields.isEmpty()) {
        showError("No fields found in the generated code. Make sure the .proto file is correctly formatted.");
        qDebug() << "Generated code:";
        qDebug() << code;
    } else {
        updateFieldInputs();
        showInfo(QString("%1 fields found in the generated code.").arg(messageFields.size()));

        qDebug() << "Fields found:";
        for (const auto& field : messageFields) {
            qDebug() << "Name:" << field.name << "Type:" << field.type << "Number:" << field.number;
        }
    }
}

void ProtobufGUI::serializeMessage() {
    if (!compileGeneratedCode()) return;
    if (!loadCompiledLibrary()) return;

    QMap<QString, QString> fieldValues;
    for (const auto &field : messageFields) {
        if (fieldInputs.contains(field.name)) {
            fieldValues[field.name] = fieldInputs[field.name]->text();
        }
    }

    if (!performSerialization(fieldValues)) return;

    showInfo("Message serialized successfully.");
}

void ProtobufGUI::loadProtoFile(const QString &filePath) {
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        protoContentEdit->setPlainText(file.readAll());
        file.close();
    } else {
        showError("Failed to open .proto file.");
    }
}

bool ProtobufGUI::validateInputs() {
    if (protocPath.isEmpty()) {
        showWarning("Please select protoc location.");
        return false;
    }

    if (protoContentEdit->toPlainText().isEmpty()) {
        showWarning("Please enter or load .proto content.");
        return false;
    }

    return true;
}

bool ProtobufGUI::writeProtoFile() {
    QString protoFilePath = outputFolderPath.isEmpty() ? tempDir->filePath("temp.proto") : QDir(outputFolderPath).filePath("temp.proto");
    QFile protoFile(protoFilePath);
    if (!protoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError("Failed to create .proto file.");
        return false;
    }
    protoFile.write(protoContentEdit->toPlainText().toUtf8());
    protoFile.close();
    return true;
}

bool ProtobufGUI::compileProtoFile() {
    QProcess process;
    QString workingDir = outputFolderPath.isEmpty() ? tempDir->path() : outputFolderPath;
    process.setWorkingDirectory(workingDir);
    process.start(protocPath, QStringList() << "--cpp_out=." << "temp.proto");
    process.waitForFinished();

    if (process.exitCode() != 0) {
        showError("Failed to compile .proto file: " + process.readAllStandardError());
        return false;
    }
    return true;
}

bool ProtobufGUI::readGeneratedCode() {
    QString workingDir = outputFolderPath.isEmpty() ? tempDir->path() : outputFolderPath;
    QString generatedHeaderPath = QDir(workingDir).filePath("temp.pb.h");
    QString generatedSourcePath = QDir(workingDir).filePath("temp.pb.cc");

    QFile headerFile(generatedHeaderPath);
    QFile sourceFile(generatedSourcePath);

    if (headerFile.open(QIODevice::ReadOnly | QIODevice::Text) &&
        sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString generatedCode = headerFile.readAll() + "\n\n" + sourceFile.readAll();
        generatedCodeEdit->setPlainText(generatedCode);
        headerFile.close();
        sourceFile.close();
        return true;
    } else {
        showError("Failed to read generated code files.");
        return false;
    }
}

// WILL FIX THIS LATER, PROBABLY...
bool ProtobufGUI::compileGeneratedCode() {
    QProcess process;
    QString workingDir = outputFolderPath.isEmpty() ? tempDir->path() : outputFolderPath;
    process.setWorkingDirectory(workingDir);

    QString protobufPath = "C:\\something should be here\\protobuf-28.2";
    QString includePath = protobufPath + "\\include";
    QString libPath = protobufPath + "\\lib";

    QStringList args;
    args << "/LD"
         << "/Fe:temp.dll"
         << "temp.pb.cc"
         << "/I."
         << "/I" + includePath
         << "libprotobuf.lib"
         << "/link"
         << "/LIBPATH:" + libPath;

    process.start("cl", args);
    process.waitForFinished(-1);

    QString stdOutput = process.readAllStandardOutput();
    QString stdError = process.readAllStandardError();

    if (process.exitCode() != 0) {
        showError("Failed to compile generated code. Exit code: " + QString::number(process.exitCode()) +
                  "\nStandard output: " + stdOutput +
                  "\nStandard error: " + stdError);
        return false;
    }

    return true;
}

bool ProtobufGUI::loadCompiledLibrary() {
    QString workingDir = outputFolderPath.isEmpty() ? tempDir->path() : outputFolderPath;
    libHandle = LoadLibrary(QDir(workingDir).filePath("temp.dll").toStdWString().c_str());
    if (!libHandle) {
        showError("Failed to load compiled library: " + QString::number(GetLastError()));
        return false;
    }
    return true;
}

//#include outputFolderPath + "\\temp.pb.h"


bool ProtobufGUI::performSerialization(const QMap<QString, QString> &fieldValues) {
    //typedef void* (__stdcall *NewMessageFunc)();
    //typedef bool (__stdcall *SetFieldFunc)(void*, const char*, const char*);
    //typedef bool (__stdcall *SerializeFunc)(const void*, std::string*);

    //NewMessageFunc newMessage = (NewMessageFunc)GetProcAddress(static_cast<HMODULE>(libHandle), "NewMessage");
    //SetFieldFunc setField = (SetFieldFunc)GetProcAddress(static_cast<HMODULE>(libHandle), "SetMessageField");
    //SerializeFunc serialize = (SerializeFunc)GetProcAddress(static_cast<HMODULE>(libHandle), "SerializeMessage");

    //if (!newMessage || !setField || !serialize) {
    //    showError("Failed to find required functions in the compiled library.");
    //    return false;
    //}

    //void* msg = newMessage();

    //for (auto it = fieldValues.constBegin(); it != fieldValues.constEnd(); ++it) {
    //    if (!setField(msg, it.key().toStdString().c_str(), it.value().toStdString().c_str())) {
    //        showError("Failed to set field: " + it.key());
    //        return false;
    //    }
    //}

    //std::string serialized;
    //if (serialize(msg, &serialized)) {
    //    serializedOutputEdit->setPlainText(QString::fromStdString(serialized.empty() ? "Empty serialized data" : serialized));
    //    return true;
    //} else {
    //    serializedOutputEdit->setPlainText("Serialization failed");
    //    return false;


    // HARDCODED ATTEMPT //

    //bool ProtobufGUI::performSerialization(const QMap<QString, QString> &fieldValues) {
        // Create a Person message
    //    example::Person person;

        // Set the fields based on user input
    //    for (auto it = fieldValues.constBegin(); it != fieldValues.constEnd(); ++it) {
    //        const QString& fieldName = it.key();
    //        const QString& fieldValue = it.value();

    //        if (fieldName == "name") {
    //            person.set_name(fieldValue.toStdString());
    //        } else if (fieldName == "age") {
    //            person.set_age(fieldValue.toInt());
    //        } else if (fieldName == "email") {
    //            person.set_email(fieldValue.toStdString());
    //        }
    //        // Add more fields as necessary
    //    }

    //    // Serialize the message
    //    std::string serialized;
    //    if (!person.SerializeToString(&serialized)) {
    //        showError("Failed to serialize the message.");
    //        return false;
    //    }

        // Display the serialized data
    //    serializedOutputEdit->setPlainText(QString::fromStdString(serialized));
          return true;
    //}
}

void ProtobufGUI::showError(const QString &message) {
    QMessageBox::critical(this, "Error", message);
}

void ProtobufGUI::showWarning(const QString &message) {
    QMessageBox::warning(this, "Warning", message);
}

void ProtobufGUI::showInfo(const QString &message) {
    QMessageBox::information(this, "Information", message);
}
