export type DisplayProfileId = '320x170' | '480x320-rgb' | '480x320-bgr';

// Optional so an older application can still use the existing display controls.
export interface IDisplaySettings {
    displayProfile?: DisplayProfileId;
    displayActiveProfile?: DisplayProfileId;
    displayProfileOptions?: string[];
    displayRestartRequired?: boolean;
    displayWidth?: number;
    displayHeight?: number;
    displayProfileCapabilityVersion?: number;
    displayProfileStorageValid?: boolean;
    displayProfileExperimental?: boolean;
}
