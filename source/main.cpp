#define TESLA_INIT_IMPL // If you have more than one file using the tesla header, only define this in the main one
#include <tesla.hpp>    // The Tesla Header

//This is a version for the SysDVR Config app protocol, it's not shown anywhere and not related to the major version
#define SYSDVR_VERSION_MIN 5
#define SYSDVR_VERSION_MAX 7
#define TYPE_MODE_USB 1
#define TYPE_MODE_TCP 2
#define TYPE_MODE_RTSP 4
#define TYPE_MODE_NULL 3
#define TYPE_MODE_SWITCHING 999998
#define TYPE_MODE_ERROR 999999

#define CMD_GET_VER 100
#define CMD_GET_MODE 101

#define MODE_TO_CMD_SET(x) x

class DvrOverlay : public tsl::Gui {
private:
    Service* dvrService;
    bool gotService = false;
    u32 version, mode, ipAddress;
    std::string modeString;
    char ipString[20];
    u32 statusColor = 0;
public:
    DvrOverlay(Service* dvrService, bool gotService) {
        this->gotService = gotService;
        this->dvrService = dvrService;
    }

    // Called when this Gui gets loaded to create the UI
    // Allocate all elements on the heap. libtesla will make sure to clean them up when not needed anymore
    virtual tsl::elm::Element* createUI() override {
        // A OverlayFrame is the base element every overlay consists of. This will draw the default Title and Subtitle.
        // If you need more information in the header or want to change it's look, use a HeaderOverlayFrame.
        auto frame = new tsl::elm::OverlayFrame(APP_TITLE, APP_VERSION);

        // A list that can contain sub elements and handles scrolling
        auto list = new tsl::elm::List();

        if(!gotService) {   
            list->addItem(getErrorDrawer("Failed to setup SysDVR Service!\nIs sysdvr running?"), getErrorDrawerSize());
            frame->setContent(list);
            return frame;
        }
        
        sysDvrGetVersion(&version);

        if(version>SYSDVR_VERSION_MAX ||version<SYSDVR_VERSION_MIN) {
            list->addItem(getErrorDrawer("Unkown SysDVR Config API: v"+ std::to_string(version) 
                +"\nOnly Config API v"+std::to_string(SYSDVR_VERSION_MIN)+" to "+std::to_string(SYSDVR_VERSION_MAX)+" is supported"), getErrorDrawerSize());
            frame->setContent(list);
            return frame;
        }

        u32 newMode, newIp;
        sysDvrGetMode(&newMode);
        nifmGetCurrentIpAddress(&newIp);
        updateMode(newMode);
        updateIP(newIp);

        auto infodrawer = new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("Info", false, x + 3, y + 16, 20, renderer->a(0xFFFF));
            renderer->drawString("Mode:", false, x + 3, y + 40, 16, renderer->a(0xFFFF));
            renderer->drawString("IP-Address:", false, x + 3, y + 60, 16, renderer->a(0xFFFF));
            
            renderer->drawCircle(x + 116, y + 35, 5, true, renderer->a(statusColor));
            renderer->drawString(modeString.c_str(), false, x + 130, y + 40, 16, renderer->a(0xFFFF));

            renderer->drawString(ipString, false, x + 110, y + 60, 16, renderer->a(0xFFFF));
        });
        list->addItem(infodrawer, 70);

        // List Items
        list->addItem(new tsl::elm::CategoryHeader("Change Mode"));

        auto *offItem = new tsl::elm::ListItem("OFF");
        offItem->setClickListener(getModeLambda(TYPE_MODE_NULL));
        list->addItem(offItem);

        auto *usbModeItem = new tsl::elm::ListItem("USB");
        usbModeItem->setClickListener(getModeLambda(TYPE_MODE_USB));
        list->addItem(usbModeItem);

        auto *tcpModeItem = new tsl::elm::ListItem("TCP");
        tcpModeItem->setClickListener(getModeLambda(TYPE_MODE_TCP));
        list->addItem(tcpModeItem);

        auto *rtspModeItem = new tsl::elm::ListItem("RTSP");
        rtspModeItem->setClickListener(getModeLambda(TYPE_MODE_RTSP));
        list->addItem(rtspModeItem);

        // Add the list to the frame for it to be drawn
        frame->setContent(list);

        // Return the frame to have it become the top level element of this Gui
        return frame;
    }

    tsl::elm::CustomDrawer* getErrorDrawer(std::string message1){
        return new tsl::elm::CustomDrawer([message1](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString(message1.c_str(), false, x + 3, y + 15, 20, renderer->a(0xF22F));
        });
    }

    int getErrorDrawerSize(){
        return 50;
    }

    std::function<bool(u64 keys)> getModeLambda(u32 mode){
        return [this,mode](u64 keys) {
            if (keys & HidNpadButton_A) {
                sysDvrSetMode(mode);
                return true;
            }
            return false;
        };
    }


    // Called once every frame to update values
    int currentFrame = 0;
    virtual void update() override {
        currentFrame++;
        //only check for dvr mode and ip cahnges every 30 fps, so 0,5-1 sec
        if(currentFrame>=30){
            currentFrame=0;
            u32 newMode, newIp;
            Result result = sysDvrGetMode(&newMode);
            if(R_SUCCEEDED(result)){
                updateMode(newMode);
            }
            nifmGetCurrentIpAddress(&newIp);
            updateIP(newIp);
        }
    }

    void updateMode(u32 newMode){
        if(newMode!=mode){
            mode = newMode;
            modeString = getModeString(mode);
            if(mode == TYPE_MODE_SWITCHING){
                statusColor = 0xF088;
            } else if(mode == TYPE_MODE_ERROR){
                statusColor = 0xF22F;
            } else if(mode == TYPE_MODE_NULL){
                statusColor = 0xF333;
            } else {
                statusColor = 0xF0F0;
            }
        }
    }

    void updateIP(u32 newIp){
        if(newIp!=ipAddress){
            ipAddress = newIp;
            snprintf(ipString, sizeof(ipString)-1, "%u.%u.%u.%u", ipAddress&0xFF, (ipAddress>>8)&0xFF, (ipAddress>>16)&0xFF, (ipAddress>>24)&0xFF);
        }
    }

    // Called once every frame to handle inputs not handled by other UI elements
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;   // Return true here to signal the inputs have been consumed
    }

    std::string getModeString(u32 mode){
        switch(mode){
            case TYPE_MODE_USB:
                return"USB";
            case TYPE_MODE_TCP:
                return"TCP";
            case TYPE_MODE_RTSP:
                return"RTSP";
            case TYPE_MODE_NULL:
                return"OFF";
            case TYPE_MODE_SWITCHING:
                return"Switching";
            case TYPE_MODE_ERROR:
                return"Error";
            default:
                return"Unkown";
        }
    }


    Result sysDvrGetVersion(u32* out_ver)
    {
        u32 val;
        Result rc = serviceDispatchOut(dvrService, CMD_GET_VER, val);
        if (R_SUCCEEDED(rc))
            *out_ver = val;
        return rc;
    }

    Result sysDvrGetMode(u32* out_mode)
    {
        u32 val;
        Result rc = serviceDispatchOut(dvrService, CMD_GET_MODE, val);
        if (R_SUCCEEDED(rc))
            *out_mode = val;
        return rc;
    }

    Result sysDvrSetMode(u32 command)
    {
        Result rs = serviceDispatch(dvrService, MODE_TO_CMD_SET(command));

        //close and reinit sysdvr service, to directly apply the new mode.
        smInitialize();
        serviceClose(dvrService);
        smGetService(dvrService, "sysdvr");
        smExit();
        return rs;
    }
};

class OverlayTest : public tsl::Overlay {
private:
    Service dvr;
    bool gotService = false;
public:
    // libtesla already initialized fs, hid, pl, pmdmnt, hid:sys and set:sys
    virtual void initServices() override {
        if(isSysDVRServiceRunning()){
            gotService = R_SUCCEEDED(smGetService(&dvr, "sysdvr"));
        }
        nifmInitialize(NifmServiceType_User);
    }  // Called at the start to initialize all services necessary for this Overlay
    virtual void exitServices() override {
        if(gotService){
            serviceClose(&dvr);
        }
        nifmExit();
    }  // Callet at the end to clean up all services previously initialized

    bool isSysDVRServiceRunning() {
      u8 tmp=0;
      SmServiceName service_name = smEncodeName("sysdvr");
      Result rc;
      if(hosversionAtLeast(12,0,0)){
        rc = tipcDispatchInOut(smGetServiceSessionTipc(), 65100, service_name, tmp);
      } else {
        rc = serviceDispatchInOut(smGetServiceSession(), 65100, service_name, tmp);
      }
      if (R_SUCCEEDED(rc) && tmp & 1)
        return true;
      else
        return false;
    }

    virtual void onShow() override {}    // Called before overlay wants to change from invisible to visible state
    virtual void onHide() override {}    // Called before overlay wants to change from visible to invisible state

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<DvrOverlay>(&dvr, gotService);  // Initial Gui to load. It's possible to pass arguments to it's constructor like this
    }
};

int main(int argc, char **argv) {
    return tsl::loop<OverlayTest>(argc, argv);
}