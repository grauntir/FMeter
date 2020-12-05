
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>


using namespace std;


// C-style samples processing
// this is a base for IRQ precedure implementation

// we need at least ten samples for the max freq 1kHz
// and enougth space for the longest signal record
#define ADC_BUF_SIZE (1024*16)
#define CLOCK_PER_SEC (10000)

// at least half period from the max freq
#define SURGE_GUARD_CNT (10)

int AdcBuf[ADC_BUF_SIZE];
unsigned LastIdx = 0;
// amplitude detection with filtering possibilities
int MaxVal = 0;
int MinVal = 0;
// edge detecting algorithm with cached values
unsigned EdgeUpCnt = 0;
unsigned EdgeDnCnt = 0;
unsigned LastEdgeUpCnt = 0;
unsigned LastEdgeDnCnt = 0;
int PrevSmpl = 0;

void ADC_IRQ(int Sample)
{
    unsigned NxtLastIdx = (LastIdx+1) % ADC_BUF_SIZE;
    AdcBuf[NxtLastIdx] = Sample;

    // some statistic will counted here

    // falldown filtering
    --MaxVal;
    ++MinVal;

    MaxVal = max(MaxVal, Sample);
    MinVal = min(MinVal, Sample);
    //todo: a DC part of the input signal could be calc here instead of zerro

    //edge detecting
    // up
    if ( (Sample > 0) and (PrevSmpl <= 0)){
        if (EdgeUpCnt > SURGE_GUARD_CNT) {
            LastEdgeUpCnt = EdgeUpCnt;
            EdgeUpCnt = 0;
        }
    }
    else {
        ++EdgeUpCnt;
    }

    //dn
    if ((Sample < 0) and (PrevSmpl >= 0)) {
        if (EdgeDnCnt > SURGE_GUARD_CNT) {
            LastEdgeDnCnt = EdgeDnCnt;
            EdgeDnCnt = 0;
        }
    }
    else {
        ++EdgeDnCnt;
    }

    PrevSmpl = Sample;
    LastIdx = NxtLastIdx;
};

int GetFreq() {
    // just for certancy increasing
    int MidleCnt = (LastEdgeUpCnt + LastEdgeDnCnt) / 2;
    // float is impossible according to technical requrements
    int Freq = 0;
    if (MidleCnt == 0) {
        // there are no valid measurements
        return 0;
    }
    else {
        Freq = CLOCK_PER_SEC / MidleCnt;

    }
    return Freq;
}

// c-style cmd processing 
// could be used as a base for UART communication

enum ECmdState {eStart, eCode, eStop} ;
#define START_SMBL (':')
#define STOP_SMBL (0x0A)

#define CMD_EXIT        (0) 
#define CMD_GET_FREQ    (1) 
#define CMD_GET_ADC     (2)


void GetFreqCmd() {
    cout << GetFreq() << endl;
}

void GetAdcCmd() {
    // Calc of start Idx for tx
    unsigned CntTotal = max(LastEdgeUpCnt, LastEdgeDnCnt);

    int StartIdx = 0;
    // logic is clear instead of logic operaton
    if (CntTotal > LastIdx) {
        // starts from the end of Buf
        StartIdx = CntTotal - LastIdx;
    }
    else {
        StartIdx = LastIdx - CntTotal;
    }

    int Cnt = 0;
    int CurIdx = StartIdx;
    cout << hex;
    while (Cnt < CntTotal) {
        int16_t Sample = AdcBuf[CurIdx];
        
        cout << "0x" <<Sample << " ";

        CurIdx = (CurIdx + 1) % ADC_BUF_SIZE;
       ++Cnt;
    };
    cout << dec << endl;
    return;
};

int CmdProcessor(char* Buf, int Len) {
    int Cnt = 0;
    ECmdState CmdState = eStart;
    ECmdState NxtCmdState = CmdState;
    char CurCh = 0;
    int CmdCode = 0;
    // Protocol parser
    while (Cnt < Len) {
        CurCh = Buf[Cnt];

        switch (CmdState) {
        case eStart:
            if (CurCh == START_SMBL) {
                NxtCmdState = eCode;
            }
            break;
        case eCode:
            if (isdigit(CurCh)) {
                CmdCode = (CmdCode *10) + (CurCh - '0');
            }
            else 
            {
                if ((CurCh == 0) || (CurCh == STOP_SMBL)) {
                    NxtCmdState = eStop;
                };
            }
            break;
        case eStop:   
            break;
        default:
            break;
        }

        if (NxtCmdState == eStop) {
            // cmd processing
            // direct output for stub
            switch (CmdCode) {
            case CMD_EXIT:
                return -1;
                break;
            case CMD_GET_FREQ:
                GetFreqCmd();
                return 0;
                break;
            case CMD_GET_ADC:
                GetAdcCmd();
                return 0;
                break;
            default:
                // wrong CMD
                return 0;
                break;
            }
            NxtCmdState = eStart;
        };

        CmdState = NxtCmdState;
        ++Cnt;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cout << "pls select a file with ADC samples as start argumnet of this app" << endl;
        return -1;
    }

    string InFileName = argv[1];
    filesystem::path CurPath = filesystem::current_path();
    string FullInFileName = (CurPath / InFileName).string();

    ifstream AdcSmpls;
 
    if (filesystem::exists(FullInFileName)) {
       AdcSmpls.open(FullInFileName, ios::binary | ios::in);
       if (!AdcSmpls) {
            cout << "\nCant open file : " << FullInFileName;
        };
    }

    // simulation of one ADC sample
    // in assumption of symetric signal for hardware abstruction
    int16_t InBuf;
    int Cnt = 0;
    int Sample = 0;
    /*vector<int16_t> Values;
    while (AdcSmpls.read(reinterpret_cast<char*>(&InBuf), sizeof(int16_t)))
    {
        Values.push_back(InBuf);
    *///};

    while (AdcSmpls.read((char*)&InBuf, sizeof(int16_t))) {
        // formating of input data
        Sample = InBuf;
        // call of IRQ immitation
        ADC_IRQ(Sample);
        ++Cnt;
    };
    

    // protocol simulation
    // 1 position - ":" - start magic sumbol
    // 2 position - "digit" - code of the command in ASCII
    // 3 position - "0x0A" - end of command (string)
    // anything else must be scipped

    enum {eMaxCmdSize = 256};
    char InCmd[eMaxCmdSize];
    while (1){
        cin.getline(InCmd, sizeof(InCmd));

        int Ret = CmdProcessor(InCmd, eMaxCmdSize);
        
        if (Ret != 0) {
            break;
        }

    }



    AdcSmpls.close();

    std::cout << "Total " << Cnt << " samples were processed in this demo application\n";
}
