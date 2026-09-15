import { Component, OnInit } from '@angular/core';
import { LayoutService } from "./service/app.layout.service";
import { BuildInfo, BuildInfoService } from '../services/build-info.service';

@Component({
    selector: 'app-footer',
    templateUrl: './app.footer.component.html',
    standalone: false
})
export class AppFooterComponent implements OnInit {
    public buildInfo: BuildInfo | null = null;

    constructor(
        public layoutService: LayoutService,
        private buildInfoService: BuildInfoService
    ) { }

    ngOnInit(): void {
        this.buildInfoService.getBuildInfo().subscribe(buildInfo => this.buildInfo = buildInfo);
    }
}
