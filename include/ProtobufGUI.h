#ifndef PROTOBUFGUI_H
#define PROTOBUFGUI_H

#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QLineEdit>
#include <QMap>
#include <QVector>
#include <memory>

class QTemporaryDir;

class ProtobufGUI : public QMainWindow {
    Q_OBJECT

public:
    ProtobufGUI(QWidget *parent = nullptr);
    ~ProtobufGUI();

private slots:
    void selectProtoFile();
    void selectProtocLocation();
    void selectOutputFolder();
    void compileProto();
    void updateFieldInputs();
    void parseGeneratedCode();
    void serializeMessage();

private:
    void setupUI();
    void connectSignalsAndSlots();
    void loadProtoFile(const QString &filePath);
    bool validateInputs();
    bool writeProtoFile();
    bool compileProtoFile();
    bool readGeneratedCode();
    bool compileGeneratedCode();
    bool loadCompiledLibrary();
    bool performSerialization(const QMap<QString, QString> &fieldValues);
    void showError(const QString &message);
    void showWarning(const QString &message);
    void showInfo(const QString &message);
    bool performSerialization();

    struct MessageField {
        QString type;
        QString name;
        int number;
    };

    QPushButton *protoFileButton;
    QPushButton *protocLocationButton;
    QPushButton *outputFolderButton;
    QPushButton *compileButton;
    QPushButton *serializeButton;
    QTextEdit *protoContentEdit;
    QPlainTextEdit *generatedCodeEdit;
    QPlainTextEdit *serializedOutputEdit;
    QScrollArea *fieldInputsScrollArea;
    QWidget *fieldInputsWidget;
    QString protocPath;
    QString outputFolderPath;
    std::unique_ptr<QTemporaryDir> tempDir;
    void* libHandle;
    QVector<MessageField> messageFields;
    QMap<QString, QLineEdit*> fieldInputs;
};

#endif // PROTOBUFGUI_H
