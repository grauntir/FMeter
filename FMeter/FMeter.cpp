/*!
\file
\brief Main file with demo application

Almost done in C++, but irq processing and command parser in c-lang
*/

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>


using namespace std;


// C-style samples processing
// this is a base for IRQ precedure implementation

// we need at least ten samples for the max freq 1kHz
// and enougth space for the longest signal record
#define ADC_BUF_SIZE (1024*1024*16)
#define CLOCK_PER_SEC (1000000)

// at least half period from the max freq
#define SURGE_GUARD_CNT (10)

int16_t AdcBuf[ADC_BUF_SIZE];
uint32_t LastIdx = 0;
// amplitude detection with filtering possibilities
int16_t MaxVal = 0;
int16_t MinVal = 0;
// edge detecting algorithm with cached values
uint32_t EdgeUpCnt = 0;
uint32_t EdgeDnCnt = 0;
uint32_t LastEdgeUpCnt = 0;
uint32_t LastEdgeDnCnt = 0;
int16_t PrevSmpl = 0;

// multipulse frequency measurements for HF
// for demonstration only rire pulse is counted
uint32_t MultiEdgeUpCnt = 0;
uint32_t LastMultiEdgeUpCnt = 0;
#define MULTI_COUNT_TOTAL (10)

// frequency measurements by counting pulse per second
// for demonstration only risind edge is processed
uint32_t OneSecPulseCnt = 0;
uint32_t UpEdgePerSecCnt = 0;
uint32_t LastUpEdgePerSecCnt = 0;

/*!
IRQ processing call
\param[in] Sample zerr0-based value from ADC without DC biasing
*/
void ADC_IRQ(int16_t Sample)
{
    uint32_t NxtLastIdx = (LastIdx+1) % ADC_BUF_SIZE;
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
            ++UpEdgePerSecCnt;
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

    //pulse-per-sec
    if (OneSecPulseCnt != CLOCK_PER_SEC) {
        ++OneSecPulseCnt;
    }
    else {
        LastUpEdgePerSecCnt = UpEdgePerSecCnt;
        UpEdgePerSecCnt = 0;
        OneSecPulseCnt = 0;
    }

    PrevSmpl = Sample;
    LastIdx = NxtLastIdx;
};


/*!
Calculation of inverce period from the last detected pulse
*/
int32_t GetFreq() {
    // float is impossible according to technical requrements
    int32_t Freq = 0;
    if ((LastEdgeUpCnt == 0) or (LastEdgeDnCnt == 0)) {
        // there are no valid measurements
        return 0;
    }
    else {
        // just for certancy increasing
        Freq = CLOCK_PER_SEC*2 / (LastEdgeDnCnt + LastEdgeUpCnt);
    }
    return Freq;
}

/*!
Calculation by direct measurements of pulse per second
*/
int32_t GetFreqFromPulse() {
    int32_t Freq = LastUpEdgePerSecCnt;
    return Freq;
}

// c-style cmd processing 
// could be used as a base for UART communication

enum ECmdState {eStart, eCode, eStop} ;
#define START_SMBL (':')
#define STOP_SMBL (0x0A)

#define CMD_EXIT            (0) 
#define CMD_GET_FREQ        (1) 
#define CMD_GET_FREQ_SEC    (2) 
#define CMD_GET_ADC         (3)



/*!
Get frequency value command processing
*/
void GetFreqCmd() {
    cout << GetFreq() << endl;
}

/*!
Get frequency value command processing
*/
void GetPulseCntFreqCmd() {
    cout << GetFreqFromPulse() << endl;
}

/*!
Get samples command processing
*/
void GetAdcCmd() {
    // Calc of start Idx for tx
    uint32_t CntTotal = max(LastEdgeUpCnt, LastEdgeDnCnt);

    uint32_t StartIdx = 0;
    // logic is clear instead of logic operaton
    if (CntTotal > LastIdx) {
        // starts from the end of Buf
        StartIdx = CntTotal - LastIdx;
    }
    else {
        StartIdx = LastIdx - CntTotal;
    }

    uint32_t Cnt = 0;
    uint32_t CurIdx = StartIdx;
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

/*!
Protocol parser implimentation
Only one cmd in time is supported at now
*/
int CmdProcessor(char* Buf, uint32_t Len) {
    uint32_t Cnt = 0;
    ECmdState CmdState = eStart;
    ECmdState NxtCmdState = CmdState;
    char CurCh = 0;
    uint32_t CmdCode = 0;
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
            case CMD_GET_FREQ_SEC:
                GetFreqCmd();
                return 0;
                break;
            case CMD_GET_ADC:
                GetPulseCntFreqCmd();
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
    uint32_t Cnt = 0;
    int16_t Sample = 0;
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
