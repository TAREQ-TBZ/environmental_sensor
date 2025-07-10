pipeline {
    agent any
    
    environment {
        ZEPHYR_TOOLCHAIN_VARIANT = 'zephyr'
        ZEPHYR_SDK_VERSION       = '0.16.3'
        ZEPHYR_SDK_INSTALL_DIR   = "${WORKSPACE}/zephyr-sdk-${ZEPHYR_SDK_VERSION}"
        NRF_SDK_VERSION          = 'v2.6.0'
        NRF_SDK_PATH             = "${WORKSPACE}/ncs"
        BOARD                    = 'nrf52840dk_nrf52840'
        APP_DIR                  = 'app'
    }

    stages {

        stage('Clean Workspace') {
            steps {
                cleanWs()
            }
        }

        stage('Checkout') {
            steps {
                checkout scm
            }
        }

        stage('Preflight Check') {
            steps {
                script {
                    sh 'ls -al'
                }
            }
        }


        stage('Install Zephyr SDK') {
            agent {
                dockerfile {
                    filename 'Dockerfile'
                    dir '.'
                    additionalBuildArgs '--build-arg USER_UID=1000 --build-arg USER_GID=1000 --build-arg USER_NAME=jenkins'
                }
            }
            steps {
                script {
                    sh '''
                    mkdir -p ws
                    python3 -m venv --copies ws/.venv
                    . ws/.venv/bin/activate
                    pip3 install west

                    west init -m git@github.com:TAREQ-TBZ/env_sensor.git --mr main ws
                    cd ws
                    west update

                    pip3 install -r zephyr/scripts/requirements-base.txt
                    pip3 install -r zephyr/scripts/requirements-extras.txt
                    pip3 install -r zephyr/scripts/requirements-build-test.txt
                    pip3 install -r nrf/scripts/requirements-base.txt
                    pip3 install -r nrf/scripts/requirements-build.txt
                    pip3 install -r bootloader/mcuboot/scripts/requirements.txt
                    '''
                }
            }
        }

        
        stage('Build Zephyr Application') {
            steps {
                script {
                    sh '''
                    cd ${WORKSPACE}
                    . ws/.venv/bin/activate
                    cd ws
                    west build -b sham_nrf52833 application/app
                    '''
                }
            }
        }

        stage('Archive Artifacts') {
            steps {
                archiveArtifacts artifacts: 'build/zephyr/*.hex, build/zephyr/*.elf', fingerprint: true
            }
        }
    }

    post {
        success {
            echo '✅ Build completed successfully!'
        }
        failure {
            echo '❌ Build failed. Please check the logs.'
        }
    }
}
